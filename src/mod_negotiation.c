#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>


#include "base.h"
#include "log.h"
#include "buffer.h"

#include "plugin.h"

#include "file_cache_funcs.h"

#include "config.h"

/* plugin config for all request/connections */

typedef struct {
	array *exts;
} plugin_config;

typedef struct {
	PLUGIN_DATA;
	
	buffer *tmp_buf;
	
	plugin_config **config_storage;
	
	plugin_config conf; 
} plugin_data;

/* init the plugin data */
INIT_FUNC(mod_negotiation_init) {
	plugin_data *p;
	
	p = calloc(1, sizeof(*p));
	
	p->tmp_buf = buffer_init();
	
	return p;
}

/* detroy the plugin data */
FREE_FUNC(mod_negotiation_free) {
	plugin_data *p = p_d;
	
	UNUSED(srv);

	if (!p) return HANDLER_GO_ON;
	
	if (p->config_storage) {
		size_t i;
		for (i = 0; i < srv->config_context->used; i++) {
			plugin_config *s = p->config_storage[i];
			
			array_free(s->exts);
			
			free(s);
		}
		free(p->config_storage);
	}
	
	buffer_free(p->tmp_buf);
	
	free(p);
	
	return HANDLER_GO_ON;
}

/* handle plugin config and check values */

SETDEFAULTS_FUNC(mod_negotiation_set_defaults) {
	plugin_data *p = p_d;
	size_t i = 0;
	
	config_values_t cv[] = { 
		{ "negotiation.extensions",     NULL, T_CONFIG_ARRAY, T_CONFIG_SCOPE_CONNECTION },       /* 0 */
		{ NULL,                         NULL, T_CONFIG_UNSET, T_CONFIG_SCOPE_UNSET }
	};
	
	if (!p) return HANDLER_ERROR;
	
	p->config_storage = calloc(1, srv->config_context->used * sizeof(specific_config *));
	
	for (i = 0; i < srv->config_context->used; i++) {
		plugin_config *s;
		
		s = calloc(1, sizeof(plugin_config));
		s->exts           = array_init();
		
		cv[0].destination = s->exts;
		
		p->config_storage[i] = s;
	
		if (0 != config_insert_values_global(srv, ((data_config *)srv->config_context->data[i])->value, cv)) {
			return HANDLER_ERROR;
		}
	}
	
	return HANDLER_GO_ON;
}

#define PATCH(x) \
	p->conf.x = s->x;
static int mod_negotiation_patch_connection(server *srv, connection *con, plugin_data *p, const char *stage, size_t stage_len) {
	size_t i, j;
	
	/* skip the first, the global context */
	for (i = 1; i < srv->config_context->used; i++) {
		data_config *dc = (data_config *)srv->config_context->data[i];
		plugin_config *s = p->config_storage[i];
		
		/* not our stage */
		if (!buffer_is_equal_string(dc->comp_key, stage, stage_len)) continue;
		
		/* condition didn't match */
		if (!config_check_cond(srv, con, dc)) continue;
		
		/* merge config */
		for (j = 0; j < dc->value->used; j++) {
			data_unset *du = dc->value->data[j];
			
			if (buffer_is_equal_string(du->key, CONST_STR_LEN("negotiation.extensions"))) {
				PATCH(exts);
			}
		}
	}
	
	return 0;
}

static int mod_negotiation_setup_connection(server *srv, connection *con, plugin_data *p) {
	plugin_config *s = p->config_storage[0];
	UNUSED(srv);
	UNUSED(con);
		
	PATCH(exts);
	
	return 0;
}
#undef PATCH

/**
 * 
 * if file doesn't exist, try to append of the the extensions and check again
 * 
 * 
 */

URIHANDLER_FUNC(mod_negotiation_docroot) {
	plugin_data *p = p_d;
	size_t k, i;
	struct stat st;
	
	if (con->uri.path->used == 0) return HANDLER_GO_ON;
	
	mod_negotiation_setup_connection(srv, con, p);
	for (i = 0; i < srv->config_patches->used; i++) {
		buffer *patch = srv->config_patches->ptr[i];
		
		mod_negotiation_patch_connection(srv, con, p, CONST_BUF_LEN(patch));
	}
	
	buffer_copy_string_buffer(p->tmp_buf, con->physical.doc_root);
	buffer_append_string_buffer(p->tmp_buf, con->physical.rel_path);
	
	/* file exists, we don't handle this */
	if (0 == stat(p->tmp_buf->ptr, &st)) return HANDLER_GO_ON;
	
	/* some other error than 'not exists' */
	if (errno != ENOENT) return HANDLER_GO_ON;
	
	if (con->conf.log_request_handling) {
		log_error_write(srv, __FILE__, __LINE__,  "s",  "-- handling the request as Negotiation");
		log_error_write(srv, __FILE__, __LINE__,  "sb", "URI          :", con->uri.path);
		log_error_write(srv, __FILE__, __LINE__,  "sb", "relative path:", con->physical.rel_path);
		log_error_write(srv, __FILE__, __LINE__,  "sb", "Docroot      :", con->physical.doc_root);
	}
	
	/* we will replace it anyway, as physical.path will change */
	for (k = 0; k < p->conf.exts->used; k++) {
		data_string *ds = (data_string *)p->conf.exts->data[k];
		
		buffer_copy_string_buffer(p->tmp_buf, con->physical.doc_root);
		buffer_append_string_buffer(p->tmp_buf, con->physical.rel_path);
		buffer_append_string_buffer(p->tmp_buf, ds->value);
		
		/* we found something */
		if (0 == stat(p->tmp_buf->ptr, &st)) {
			buffer_append_string_buffer(con->physical.rel_path, ds->value);
			return HANDLER_GO_ON;
		}
		
		if (errno != ENOENT) return HANDLER_GO_ON;
	}
	
	/* not found */
	return HANDLER_GO_ON;
}

/* this function is called at dlopen() time and inits the callbacks */

int mod_negotiation_plugin_init(plugin *p) {
	p->version     = LIGHTTPD_VERSION_ID;
	p->name        = buffer_init_string("negotiation");
	
	p->init        = mod_negotiation_init;
	p->handle_docroot = mod_negotiation_docroot;
	p->set_defaults  = mod_negotiation_set_defaults;
	p->cleanup     = mod_negotiation_free;
	
	p->data        = NULL;
	
	return 0;
}
