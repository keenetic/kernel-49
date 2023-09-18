#include <linux/version.h>
#include <linux/skbuff.h>

#include "br_private.h"

#if !defined(ETH_P_LLDP)
#define ETH_P_LLDP					0x88cc
#endif /* ETH_P_LLDP */

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 15, 0)
#define timer_setup(timer, func, flags)					\
	setup_timer(timer, (void (*)(unsigned long))func,		\
		    (unsigned long)timer)
#define from_timer(var, timer, field)					\
	container_of(timer, typeof(*var), field)
#endif

static inline u32 br_queue_ewma(u32 avg, u32 sample, u16 shift)
{
	avg -= avg >> shift;
	avg += sample >> shift;
	return avg;
}

static inline bool br_red_should_drop(const unsigned int min,
				      const struct reciprocal_value *prob,
				      const unsigned int val)
{
	if (val > min) {
		const unsigned int p = prandom_u32() & 0xff;

		if (reciprocal_divide((val - min) << 8, *prob) > p)
			return true;
	}

	return false;
}

static unsigned int br_enqueue_(struct br_queue *q, struct sk_buff *skb)
{
	const u32 new_qlen = skb_queue_len(&q->packets) + 1;
	const bool queue_was_empty = (new_qlen == 1);
	const unsigned long req_bytes = q->burst_bytes + skb->len;
	const unsigned long new_bytes = q->packets_size + skb->len;
	const unsigned long req_jiffies = req_bytes / q->bpj;
	const unsigned long burst_bytes = req_bytes % q->bpj;
	const unsigned long now = jiffies;
	unsigned long send_time;

	if (time_before(q->send_time + req_jiffies, now)) {
		/* no packet queueing required */
		q->send_time = now;

		if (time_before(q->send_time + HZ, now))
			q->burst_bytes = skb->len % q->bpj; /* new sequence */
		else
			q->burst_bytes = burst_bytes;

		return NF_ACCEPT;
	}

	if (new_bytes > q->limit_bytes) {
		++q->red_hard_drop;
		goto red_drop;
	}

	if (new_qlen > q->limit_pkts) {
		++q->red_hard_drop;
		goto red_drop;
	}

	if (eth_hdr(skb)->h_proto != ntohs(ETH_P_LLDP)) {
		if (br_red_should_drop(q->min_bytes, &q->prob_bytes,
				       q->packets_size)) {
			++q->red_cong_drop;
			goto red_drop;
		}

		if (br_red_should_drop(q->min_pkts, &q->prob_pkts, new_qlen)) {
			++q->red_cong_drop;
			goto red_drop;
		}
	}

	send_time = q->send_time + req_jiffies;

	BR_INPUT_SKB_CB(skb)->send_time = send_time;

	__skb_queue_tail(&q->packets, skb);

	q->packets_size = new_bytes;

	q->send_time = send_time;
	q->burst_bytes = burst_bytes;

	q->avg_bytes = br_queue_ewma(q->avg_bytes, q->packets_size, 1);
	q->avg_pkts = br_queue_ewma(q->avg_pkts, new_qlen, 1);

	if (queue_was_empty)
		mod_timer(&q->timer, q->send_time);

	return NF_STOLEN;

red_drop:

	q->avg_bytes = br_queue_ewma(q->avg_bytes, q->packets_size, 1);
	q->avg_pkts = br_queue_ewma(q->avg_pkts, skb_queue_len(&q->packets), 1);

	return NF_DROP;
}

unsigned int br_enqueue(struct br_queue *q, struct sk_buff *skb)
{
	unsigned int verdict;

	spin_lock_bh(&q->lock);
	verdict = br_enqueue_(q, skb);
	spin_unlock_bh(&q->lock);

	return verdict;
}

static void br_queue_on_timer_(struct timer_list *timer)
{
	struct br_queue *q = from_timer(q, timer, timer);

	spin_lock_bh(&q->lock);

	while (likely(skb_queue_len(&q->packets) > 0)) {
		struct sk_buff *skb = __skb_dequeue(&q->packets);
		const unsigned long send_time = BR_INPUT_SKB_CB(skb)->send_time;

		if (likely(time_before_eq(send_time, jiffies))) {
			q->packets_size -= skb->len;

			spin_unlock_bh(&q->lock);

			rcu_read_lock();
			q->on_dequeue(skb);
			rcu_read_unlock();

			spin_lock_bh(&q->lock);
		} else {
			if (!atomic_read(&q->timer_shutdown)) {
				__skb_queue_head(&q->packets, skb);
				mod_timer(&q->timer, send_time);
			}

			break;
		}
	}

	spin_unlock_bh(&q->lock);
}

static void br_queue_set_kbps(struct br_queue *q, const uint32_t kbps)
{
	unsigned long long speed = 1000ULL * (unsigned long long) kbps;

	do_div(speed, 8 * HZ);

	spin_lock_bh(&q->lock);
	q->bpj = (unsigned long) speed;
	spin_unlock_bh(&q->lock);
}

void br_queue_init(struct br_queue *q, void (*on_dequeue)(struct sk_buff *))
{
	memset(q, 0, sizeof(*q));

	spin_lock_init(&q->lock);

	__skb_queue_head_init(&q->packets);

	timer_setup(&q->timer, br_queue_on_timer_, q);

	atomic_set(&q->timer_shutdown, 0);
	smp_wmb(); /* a barrier required after atomic_set() */

	br_queue_set_kbps(q, 640); /* 640 kbps */
	q->limit_pkts = 1024;
	q->min_pkts = 512;

	q->limit_bytes = 48 * 1024;
	q->min_bytes = 12 * 1024;

	q->send_time = jiffies - HZ;
	q->prob_pkts = reciprocal_value(max(q->limit_pkts - q->min_pkts, 1U));
	q->prob_bytes = reciprocal_value(max(q->limit_bytes - q->min_bytes, 1U));

	q->on_dequeue = on_dequeue;
}

void br_queue_destroy(struct br_queue *q)
{
	struct sk_buff *curr, *next;

	/* this must be set before synchronous stop */
	atomic_inc(&q->timer_shutdown);
	del_timer_sync(&q->timer);
	atomic_dec(&q->timer_shutdown);

	spin_lock_bh(&q->lock);

	skb_queue_walk_safe(&q->packets, curr, next) {
		__skb_unlink(curr, &q->packets);
		kfree_skb(curr);
	}

	spin_unlock_bh(&q->lock);
}

int br_queue_overloaded(struct br_queue *q)
{
	return
		(q->avg_pkts > q->limit_pkts * 4 / 5) ||
		(q->avg_bytes > q->limit_bytes * 4 / 5);
}
