// SPDX-License-Identifier: GPL-2.0
/* Load-based CPU frequency profiles for The Bakelite Project. */

#include <linux/init.h>
#include <linux/math64.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/jiffies.h>

#include "cpufreq_governor.h"

struct bakelite_profile {
	unsigned int target_load;
	unsigned int floor_load;
	unsigned int down_samples;
	unsigned int sampling_rate_us;
};

struct bakelite_policy {
	struct policy_dbs_info policy_dbs;
	unsigned int down_count;
	unsigned long boost_until;
};

struct bakelite_tuners {
	unsigned int target_load;
	unsigned int floor_load;
	unsigned int down_samples;
	unsigned int input_boost_ms;
	bool profile_initialized;
};

struct bakelite_governor {
	struct dbs_governor dbs;
	const struct bakelite_profile *profile;
};

static const struct bakelite_profile perf_profile = {
	.target_load = 65,
	.floor_load = 15,
	.down_samples = 3,
	.sampling_rate_us = 10000,
};

static const struct bakelite_profile balance_profile = {
	.target_load = 80,
	.floor_load = 0,
	.down_samples = 2,
	.sampling_rate_us = 20000,
};

static const struct bakelite_profile battery_profile = {
	.target_load = 95,
	.floor_load = 0,
	.down_samples = 1,
	.sampling_rate_us = 40000,
};

static const struct bakelite_profile *bakelite_profile_of(struct cpufreq_policy *policy)
{
	struct dbs_governor *dbs = dbs_governor_of(policy);

	return container_of(dbs, struct bakelite_governor, dbs)->profile;
}

static unsigned int bakelite_update(struct cpufreq_policy *policy)
{
	struct policy_dbs_info *policy_dbs = policy->governor_data;
	struct bakelite_policy *state = container_of(policy_dbs,
						struct bakelite_policy, policy_dbs);
	struct dbs_data *data = policy_dbs->dbs_data;
	const struct bakelite_profile *profile = bakelite_profile_of(policy);
	struct bakelite_tuners *tuners = data->tuners;
	unsigned int load = max(dbs_update(policy), tuners->floor_load);
	unsigned int demand = min(100U, load * 100 / tuners->target_load);
	unsigned int target = policy->min +
		div_u64((u64)(policy->max - policy->min) * demand, 100);

	/* Treat the first busy wakeup as a short input burst. */
	if (tuners->input_boost_ms && load >= tuners->target_load &&
	    !time_before(jiffies, state->boost_until))
		state->boost_until = jiffies +
			msecs_to_jiffies(min(tuners->input_boost_ms, 2000U));
	if (time_before(jiffies, state->boost_until))
		target = policy->max;

	/* Hold a higher frequency briefly without delaying a new load spike. */
	if (target < policy->cur && policy->cur <= policy->max &&
	    ++state->down_count < tuners->down_samples)
		return data->sampling_rate;

	state->down_count = 0;
	if (target != policy->cur)
		__cpufreq_driver_target(policy, target, CPUFREQ_RELATION_C);

	return data->sampling_rate;
}

static struct policy_dbs_info *bakelite_alloc(void)
{
	struct bakelite_policy *state = kzalloc(sizeof(*state), GFP_KERNEL);

	return state ? &state->policy_dbs : NULL;
}

static void bakelite_free(struct policy_dbs_info *policy_dbs)
{
	kfree(container_of(policy_dbs, struct bakelite_policy, policy_dbs));
}

static int bakelite_init(struct dbs_data *data)
{
	struct bakelite_tuners *tuners;

	tuners = kzalloc(sizeof(*tuners), GFP_KERNEL);
	if (!tuners)
		return -ENOMEM;
	tuners->target_load = balance_profile.target_load;
	tuners->floor_load = balance_profile.floor_load;
	tuners->down_samples = balance_profile.down_samples;
	data->tuners = tuners;
	data->ignore_nice_load = 0;
	/* Include I/O wait time in load accounting for app launch response. */
	data->io_is_busy = 1;
	data->sampling_down_factor = 1;
	return 0;
}

static void bakelite_exit(struct dbs_data *data)
{
	kfree(data->tuners);
	data->tuners = NULL;
}

static void bakelite_start(struct cpufreq_policy *policy)
{
	struct policy_dbs_info *policy_dbs = policy->governor_data;
	struct bakelite_policy *state = container_of(policy_dbs,
						struct bakelite_policy, policy_dbs);
	struct dbs_data *data = policy_dbs->dbs_data;
	const struct bakelite_profile *profile = bakelite_profile_of(policy);
	struct bakelite_tuners *tuners = data->tuners;
	unsigned int minimum_rate = max(profile->sampling_rate_us,
				cpufreq_policy_transition_delay_us(policy));

	state->down_count = 0;
	state->boost_until = 0;
	if (!tuners->profile_initialized) {
		tuners->target_load = profile->target_load;
		tuners->floor_load = profile->floor_load;
		tuners->down_samples = profile->down_samples;
		tuners->profile_initialized = true;
	}
	data->sampling_rate = max(data->sampling_rate, minimum_rate);
}

static ssize_t bakelite_show_target_load(struct gov_attr_set *attr_set, char *buf)
{
	return sprintf(buf, "%u\n", to_dbs_data(attr_set)->tuners ?
		((struct bakelite_tuners *)to_dbs_data(attr_set)->tuners)->target_load : 0);
}

static ssize_t bakelite_show_floor_load(struct gov_attr_set *attr_set, char *buf)
{
	return sprintf(buf, "%u\n", ((struct bakelite_tuners *)
		to_dbs_data(attr_set)->tuners)->floor_load);
}

static ssize_t bakelite_show_down_samples(struct gov_attr_set *attr_set, char *buf)
{
	return sprintf(buf, "%u\n", ((struct bakelite_tuners *)
		to_dbs_data(attr_set)->tuners)->down_samples);
}

static ssize_t bakelite_show_input_boost_ms(struct gov_attr_set *attr_set, char *buf)
{
	return sprintf(buf, "%u\n", ((struct bakelite_tuners *)
		to_dbs_data(attr_set)->tuners)->input_boost_ms);
}

static ssize_t bakelite_show_sampling_rate(struct gov_attr_set *attr_set,
						char *buf)
{
	return sprintf(buf, "%u\n", to_dbs_data(attr_set)->sampling_rate);
}

static ssize_t bakelite_store_target_load(struct gov_attr_set *attr_set,
					 const char *buf, size_t count)
{
	struct bakelite_tuners *tuners = to_dbs_data(attr_set)->tuners;
	unsigned int value;

	if (kstrtouint(buf, 10, &value) || value < 1 || value > 100 ||
	    value <= tuners->floor_load)
		return -EINVAL;
	tuners->target_load = value;
	return count;
}

static ssize_t bakelite_store_floor_load(struct gov_attr_set *attr_set,
					const char *buf, size_t count)
{
	struct bakelite_tuners *tuners = to_dbs_data(attr_set)->tuners;
	unsigned int value;

	if (kstrtouint(buf, 10, &value) || value > 99 ||
	    value >= tuners->target_load)
		return -EINVAL;
	tuners->floor_load = value;
	return count;
}

static ssize_t bakelite_store_down_samples(struct gov_attr_set *attr_set,
					  const char *buf, size_t count)
{
	struct bakelite_tuners *tuners = to_dbs_data(attr_set)->tuners;
	unsigned int value;

	if (kstrtouint(buf, 10, &value) || value < 1 || value > 20)
		return -EINVAL;
	tuners->down_samples = value;
	return count;
}

static ssize_t bakelite_store_input_boost_ms(struct gov_attr_set *attr_set,
					    const char *buf, size_t count)
{
	struct bakelite_tuners *tuners = to_dbs_data(attr_set)->tuners;
	unsigned int value;

	if (kstrtouint(buf, 10, &value) || value > 2000)
		return -EINVAL;
	tuners->input_boost_ms = value;
	return count;
}

static struct governor_attr bakelite_target_load =
	__ATTR(target_load, 0644, bakelite_show_target_load, bakelite_store_target_load);
static struct governor_attr bakelite_floor_load =
	__ATTR(floor_load, 0644, bakelite_show_floor_load, bakelite_store_floor_load);
static struct governor_attr bakelite_down_samples =
	__ATTR(down_samples, 0644, bakelite_show_down_samples,
	       bakelite_store_down_samples);
static struct governor_attr bakelite_input_boost_ms =
	__ATTR(input_boost_ms, 0644, bakelite_show_input_boost_ms,
	       bakelite_store_input_boost_ms);
static struct governor_attr bakelite_sampling_rate =
	__ATTR(sampling_rate, 0644, bakelite_show_sampling_rate, store_sampling_rate);

static struct attribute *bakelite_attributes[] = {
	&bakelite_target_load.attr,
	&bakelite_floor_load.attr,
	&bakelite_down_samples.attr,
	&bakelite_input_boost_ms.attr,
	&bakelite_sampling_rate.attr,
	NULL
};

#define BAKELITE_GOVERNOR(_name, _profile) { \
	.dbs = { \
		.gov = CPUFREQ_DBS_GOVERNOR_INITIALIZER(_name), \
		.kobj_type = { .default_attrs = bakelite_attributes }, \
		.gov_dbs_update = bakelite_update, \
		.alloc = bakelite_alloc, \
		.free = bakelite_free, \
		.init = bakelite_init, \
		.exit = bakelite_exit, \
		.start = bakelite_start, \
	}, \
	.profile = &_profile, \
}

static struct bakelite_governor perf_gov =
	BAKELITE_GOVERNOR("bakeliteperf", perf_profile);
static struct bakelite_governor balance_gov =
	BAKELITE_GOVERNOR("bakelitebalance", balance_profile);
static struct bakelite_governor battery_gov =
	BAKELITE_GOVERNOR("bakelitebattery", battery_profile);

#if defined(CONFIG_CPU_FREQ_DEFAULT_GOV_BAKELITEPERF)
struct cpufreq_governor *cpufreq_default_governor(void)
{
	return &perf_gov.dbs.gov;
}
#elif defined(CONFIG_CPU_FREQ_DEFAULT_GOV_BAKELITEBATTERY)
struct cpufreq_governor *cpufreq_default_governor(void)
{
	return &battery_gov.dbs.gov;
}
#elif defined(CONFIG_CPU_FREQ_DEFAULT_GOV_BAKELITEBALANCE)
struct cpufreq_governor *cpufreq_default_governor(void)
{
	return &balance_gov.dbs.gov;
}
#endif

static int __init bakelite_register(void)
{
	int ret;

	ret = cpufreq_register_governor(&perf_gov.dbs.gov);
	if (ret)
		return ret;

	ret = cpufreq_register_governor(&balance_gov.dbs.gov);
	if (ret)
		goto unregister_perf;

	ret = cpufreq_register_governor(&battery_gov.dbs.gov);
	if (!ret)
		return 0;

	cpufreq_unregister_governor(&balance_gov.dbs.gov);
unregister_perf:
	cpufreq_unregister_governor(&perf_gov.dbs.gov);
	return ret;
}
fs_initcall(bakelite_register);

MODULE_DESCRIPTION("The Bakelite Project performance, balance and battery CPUFreq governors");
MODULE_LICENSE("GPL");
