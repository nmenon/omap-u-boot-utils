#ifndef __COMPARE__
#define __COMPARE__

#define TYPE_U8		1
#define TYPE_U16	2
#define TYPE_U32	3
#define TYPE_S		4

struct compare_map {
	char *name;
	void *variable;
	int type;
};
static struct compare_map *variable_map_g;
static int size_var_map;
/**
 * @brief compare_eventhandler - this is the parser logic
 *
 * This provides a hook for the lcfg library to call when a
 * match is found for a specific keypair
 *
 * @param key key string
 * @param data data buffer
 * @param len length of the data buffer
 * @param user_data any specifics
 *
 * @return error if it did not match our dictionary or a data overflow,
 * else success
 */
static enum lcfg_status compare_eventhandler(const char *key, void *data, size_t len, void *user_data)
{
	int i;
	struct compare_map *c = NULL;
	const char *str = (const char *)data;
	int ret = 0;
	unsigned int val = 1, max_val;
	for (i = 0; i < size_var_map; i++) {
		c = &variable_map_g[i];
		if (!strcmp(c->name, key)) {
			val = 0;
			break;
		}
	}

	if (val) {
		APP_ERROR("Skipping unused '%s' in config file\n", key)
		    return lcfg_status_ok;
	}
	switch (c->type) {
	case TYPE_S:
		strcpy((char *)c->variable, (char *)data);
		break;
	case TYPE_U32:
	case TYPE_U16:
	case TYPE_U8:
		ret = sscanf(str, "%x", &val);
		if (!ret) {
			APP_ERROR("scanf failed for key %s and data %s\n", key,
				  str)
			    return lcfg_status_ok;
		}
		max_val = 0xFFFFFFFF;
		if (c->type == TYPE_U32)
			*((unsigned int *)c->variable) = val;
		else if (c->type == TYPE_U16) {
			max_val = 0xFFFF;
			*((unsigned short *)c->variable) = (unsigned short)val;
		} else if (c->type == TYPE_U8) {
			max_val = 0xFF;
			*((unsigned char *)c->variable) = (unsigned char)val;
		} else {
			APP_ERROR("%s:%s():%d:Key %s type %d unhandled->"
				  "please email developer with this message\n",
				  __FILE__, __FUNCTION__, __LINE__, c->name,
				  c->type)
		}
		if (val > max_val) {
			APP_ERROR("Config File error: in key %s, value 0x%08X "
				  "is greater than maximum allowed value 0x%08X"
				  "\n", c->name, val, max_val)
			    return lcfg_status_error;
		}

		break;
	default:
		APP_ERROR("Unknown type??\n")
		    return lcfg_status_error;

	}
	return lcfg_status_ok;
}
#endif /* __COMPARE__ */
