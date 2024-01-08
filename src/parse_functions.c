/*
* Copyright 2019 Tresys Technology, LLC
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*    http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#include <stdlib.h>
#include <string.h>

#include "parse_functions.h"
#include "selint_error.h"
#include "tree.h"
#include "template.h"
#include "util.h"
#include "perm_macro.h"
#include "xalloc.h"

static char *module_name = NULL;

enum selint_error insert_header(struct policy_node **cur, const char *mn,
                                enum header_flavor flavor, unsigned int lineno)
{
	struct header_data *data = (struct header_data *)xmalloc(sizeof(struct header_data));
	if (!data) {
		return SELINT_OUT_OF_MEM;
	}

	memset(data, 0, sizeof(struct header_data));

	data->flavor = flavor;
	data->module_name = xstrdup(mn);
	if (!data->module_name) {
		free(data);
		return SELINT_OUT_OF_MEM;
	}

	union node_data nd;
	nd.h_data = data;

	enum selint_error ret = insert_policy_node_next(*cur, NODE_HEADER, nd, lineno);
	if (ret != SELINT_SUCCESS) {
		return ret;
	}
	*cur = (*cur)->next;

	return SELINT_SUCCESS;
}

void set_current_module_name(const char *mn)
{
	if (module_name != NULL) {
		free(module_name);
	}
	module_name = xstrdup(mn);
}

char *get_current_module_name(void)
{
	return module_name;
}

enum selint_error insert_comment(struct policy_node **cur, unsigned int lineno)
{
	union node_data data;

	data.str = NULL;
	enum selint_error ret = insert_policy_node_next(*cur, NODE_COMMENT, data, lineno);
	if (ret != SELINT_SUCCESS) {
		return ret;
	}
	*cur = (*cur)->next;

	return SELINT_SUCCESS;
}

enum selint_error insert_declaration(struct policy_node **cur,
                                     enum decl_flavor flavor,
                                     const char *name,
                                     struct string_list *attrs,
                                     unsigned int lineno)
{

	if (!is_in_require(*cur)) {
		// In a require block, the objects aren't being declared
		// Otherwise, we need to insert them into the appropriate map

		const char *temp_name = get_name_if_in_template(*cur);

		if (temp_name) {
			// We are inside a template, so we need to save declarations in the template map
			// 'role foo types bar_t, baz_t;' statements are not declarations.
			insert_decl_into_template_map(temp_name, flavor, name);
		} else if ('$' != name[0]) {
			// If the name starts with $ we're probably doing something like associating
			// a role with types in interfaces

			const char *mn = get_current_module_name();

			if (!mn) {
				return SELINT_NO_MOD_NAME;
			}

			insert_into_decl_map(name, mn, flavor);

		}
	}

	struct declaration_data *data = (struct declaration_data *)xmalloc(sizeof(struct declaration_data));
	if (!data) {
		return SELINT_OUT_OF_MEM;
	}

	memset(data, 0, sizeof(struct declaration_data));

	data->flavor = flavor;
	data->name = xstrdup(name);
	data->attrs = attrs;

	union node_data nd;
	nd.d_data = data;

	enum selint_error ret =
		insert_policy_node_next(*cur, NODE_DECL, nd, lineno);

	if (ret != SELINT_SUCCESS) {
		free(data);
		return ret;
	}

	*cur = (*cur)->next;

	return SELINT_SUCCESS;
}

enum selint_error insert_aliases(struct policy_node **cur,
                                 struct string_list *aliases,
                                 enum decl_flavor flavor, unsigned int lineno)
{

	struct string_list *alias = aliases;

	while (alias) {
		const char *temp_name = get_name_if_in_template(*cur);
		if (temp_name) {
			insert_decl_into_template_map(temp_name, flavor,
			                              alias->string);
		} else {
			const char *mn = get_current_module_name();
			if (!mn) {
				free_string_list(aliases);
				return SELINT_NO_MOD_NAME;
			}

			insert_into_decl_map(alias->string, mn, flavor);
		}
		union node_data nd;
		nd.str = xstrdup(alias->string);
		enum selint_error ret = insert_policy_node_child(*cur,
		                                                 NODE_ALIAS,
		                                                 nd,
		                                                 lineno);
		if (ret != SELINT_SUCCESS) {
			return ret;
		}
		alias = alias->next;
	}

	free_string_list(aliases);

	return SELINT_SUCCESS;
}

enum selint_error insert_type_alias(struct policy_node **cur, const char *type,
                                    unsigned int lineno)
{

	union node_data nd;

	nd.str = xstrdup(type);
	enum selint_error ret = insert_policy_node_next(*cur,
	                                                NODE_TYPE_ALIAS,
	                                                nd,
	                                                lineno);
	if (ret != SELINT_SUCCESS) {
		return ret;
	}

	*cur = (*cur)->next;
	return SELINT_SUCCESS;
}

enum selint_error insert_av_rule(struct policy_node **cur,
                                 enum av_rule_flavor flavor,
                                 struct string_list *sources,
                                 struct string_list *targets,
                                 struct string_list *object_classes,
                                 struct string_list *perms, unsigned int lineno)
{

	struct av_rule_data *av_data = xmalloc(sizeof(struct av_rule_data));

	av_data->flavor = flavor;
	av_data->sources = sources;
	av_data->targets = targets;
	av_data->object_classes = object_classes;
	av_data->perms = perms;

	union node_data nd;
	nd.av_data = av_data;

	if ((*cur)->parent && (*cur)->parent->flavor == NODE_INTERFACE_DEF &&
	    str_in_sl("associate", perms)) {
		mark_transform_if((*cur)->parent->data.str);
	}

	enum selint_error ret = insert_policy_node_next(*cur,
	                                                NODE_AV_RULE,
	                                                nd,
	                                                lineno);
	if (ret != SELINT_SUCCESS) {
		free_av_rule_data(av_data);
		return ret;
	}

	*cur = (*cur)->next;

	return SELINT_SUCCESS;
}

enum selint_error insert_xperm_av_rule(struct policy_node **cur,
                                       enum av_rule_flavor flavor,
                                       struct string_list *sources,
                                       struct string_list *targets,
                                       struct string_list *object_classes,
                                       const char *operation,
                                       struct string_list *perms,
                                       unsigned int lineno)
{

	struct xav_rule_data *xav_data = xmalloc(sizeof(struct xav_rule_data));

	xav_data->flavor = flavor;
	xav_data->sources = sources;
	xav_data->targets = targets;
	xav_data->object_classes = object_classes;
	xav_data->operation = xstrdup(operation);
	xav_data->perms = perms;

	union node_data nd;
	nd.xav_data = xav_data;

	enum selint_error ret = insert_policy_node_next(*cur,
	                                                NODE_XAV_RULE,
	                                                nd,
	                                                lineno);
	if (ret != SELINT_SUCCESS) {
		free_xav_rule_data(xav_data);
		return ret;
	}

	*cur = (*cur)->next;

	return SELINT_SUCCESS;
}


enum selint_error insert_role_allow(struct policy_node **cur,
                                    struct string_list *from_roles,
                                    struct string_list *to_roles, unsigned int lineno)
{
	struct role_allow_data *ra_data = xmalloc(sizeof(struct role_allow_data));

	ra_data->from = from_roles;
	ra_data->to = to_roles;

	union node_data nd;
	nd.ra_data = ra_data;

	enum selint_error ret =
		insert_policy_node_next(*cur, NODE_ROLE_ALLOW, nd, lineno);
	if (ret != SELINT_SUCCESS) {
		free_ra_data(ra_data);
		return ret;
	}

	*cur = (*cur)->next;

	return SELINT_SUCCESS;
}

enum selint_error insert_role_types(struct policy_node **cur, const char *role,
                                    struct string_list *types, unsigned int lineno)
{
	if ((*cur)->parent && (*cur)->parent->flavor == NODE_INTERFACE_DEF) {
		const struct string_list *cur_sl_item = types;
		while (cur_sl_item) {
			if (cur_sl_item->string[0] == '$') {
				// Role interfaces are only those where the types are passed in, not the roles
				mark_role_if((*cur)->parent->data.str);
				break;
			}
			cur_sl_item = cur_sl_item->next;
		}
	}

	struct role_types_data *rtyp_data = (struct role_types_data *)xmalloc(sizeof(struct role_types_data));

	rtyp_data->role = xstrdup(role);
	rtyp_data->types = types;

	union node_data nd;
	nd.rtyp_data = rtyp_data;

	enum selint_error ret = insert_policy_node_next(*cur, NODE_ROLE_TYPES, nd, lineno);
	if (ret != SELINT_SUCCESS) {
		free_rtyp_data(rtyp_data);
		return ret;
	}

	*cur = (*cur)->next;

	return SELINT_SUCCESS;
}

enum selint_error insert_type_transition(struct policy_node **cur,
                                         enum tt_flavor flavor,
                                         struct string_list *sources,
                                         struct string_list *targets,
                                         struct string_list *object_classes,
                                         const char *default_type, const char *name,
                                         unsigned int lineno)
{

	struct type_transition_data *tt_data =
		xmalloc(sizeof(struct type_transition_data));

	tt_data->sources = sources;
	tt_data->targets = targets;
	tt_data->object_classes = object_classes;
	tt_data->default_type = xstrdup(default_type);
	if (name) {
		tt_data->name = xstrdup(name);
	} else {
		tt_data->name = NULL;
	}
	tt_data->flavor = flavor;

	if (!str_in_sl("process", object_classes) &&
	    (*cur)->parent &&
	    (*cur)->parent->flavor == NODE_INTERFACE_DEF) {
		mark_filetrans_if((*cur)->parent->data.str);
	}

	union node_data nd;
	nd.tt_data = tt_data;

	enum selint_error ret = insert_policy_node_next(*cur,
	                                                NODE_TT_RULE,
	                                                nd,
	                                                lineno);
	if (ret != SELINT_SUCCESS) {
		free_type_transition_data(tt_data);
		return ret;
	}

	*cur = (*cur)->next;

	return SELINT_SUCCESS;
}

enum selint_error insert_role_transition(struct policy_node **cur,
                                         struct string_list *sources,
                                         struct string_list *targets,
                                         struct string_list *object_classes,
                                         const char *default_role,
                                         unsigned int lineno)
{
	struct role_transition_data *rt_data =
	        xmalloc(sizeof(struct role_transition_data));

	rt_data->sources = sources;
	rt_data->targets = targets;
	rt_data->object_classes = object_classes;
	rt_data->default_role = xstrdup(default_role);

	union node_data nd;
	nd.rt_data = rt_data;

	enum selint_error ret = insert_policy_node_next(*cur,
	                                                NODE_RT_RULE,
	                                                nd,
	                                                lineno);

	if (ret != SELINT_SUCCESS) {
		free_role_transition_data(rt_data);
		return ret;
	}

	*cur = (*cur)->next;

	return SELINT_SUCCESS;
}

static int is_filetrans_if_name(const char *if_name)
{
	if (0 == strcmp(if_name, "filetrans_pattern")) {
		return 1;
	}

	if (0 == strcmp(if_name, "filetrans_add_pattern")) {
		return 1;
	}

	const char *suffix = strrchr(if_name, '_');
	if (suffix &&
	    (0 == strcmp(suffix, "_filetrans"))) {
		return 1;
	}

	return 0;
}

enum selint_error insert_interface_call(struct policy_node **cur, const char *if_name,
                                        struct string_list *args,
                                        unsigned int lineno)
{
	struct if_call_data *if_data = xmalloc(sizeof(struct if_call_data));

	if_data->name = xstrdup(if_name);
	if_data->args = args;

	const char *template_name = get_name_if_in_template(*cur);

	if (template_name) {
		insert_call_into_template_map(template_name, if_data);
	} else if (!is_in_if_define(*cur)) {
		enum selint_error r = add_template_declarations(if_name, args, NULL, module_name);
		if (r != SELINT_SUCCESS) {
			free_if_call_data(if_data);
			return r;
		}
	}

	if (is_filetrans_if_name(if_name) &&
	    (*cur)->parent &&
	    (*cur)->parent->flavor == NODE_INTERFACE_DEF) {
		mark_filetrans_if((*cur)->parent->data.str);
	}

	mark_used_if(if_name);

	union node_data nd;
	nd.ic_data = if_data;
	enum selint_error ret = insert_policy_node_next(*cur,
	                                                NODE_IF_CALL,
	                                                nd,
	                                                lineno);
	if (ret != SELINT_SUCCESS) {
		free_if_call_data(if_data);
		return ret;
	}

	*cur = (*cur)->next;

	return SELINT_SUCCESS;
}

enum selint_error insert_permissive_statement(struct policy_node **cur,
                                              const char *domain, unsigned int lineno)
{
	union node_data nd;

	nd.str = xstrdup(domain);
	enum selint_error ret = insert_policy_node_next(*cur,
	                                                NODE_PERMISSIVE,
	                                                nd,
	                                                lineno);
	if (ret != SELINT_SUCCESS) {
		return ret;
	}

	*cur = (*cur)->next;
	return SELINT_SUCCESS;
}

enum selint_error insert_semicolon(struct policy_node **cur, unsigned int lineno)
{

	union node_data nd;
	nd.str = NULL;

	enum selint_error ret = insert_policy_node_next(*cur,
	                                                NODE_SEMICOLON,
	                                                nd,
	                                                lineno);
	if (ret != SELINT_SUCCESS) {
		return ret;
	}

	*cur = (*cur)->next;
	return SELINT_SUCCESS;
}

enum selint_error insert_m4simplemacro(struct policy_node **cur, char *name, unsigned int lineno)
{

	union node_data nd;
	nd.str = name;

	enum selint_error ret = insert_policy_node_next(*cur,
	                                                NODE_M4_SIMPLE_MACRO,
	                                                nd,
	                                                lineno);
	if (ret != SELINT_SUCCESS) {
		return ret;
	}

	*cur = (*cur)->next;
	return SELINT_SUCCESS;
}

static enum selint_error begin_block(struct policy_node **cur,
                              enum node_flavor block_type, union node_data nd,
                              unsigned int lineno)
{
	enum selint_error ret = insert_policy_node_next(*cur,
	                                                block_type,
	                                                nd,
	                                                lineno);

	if (ret != SELINT_SUCCESS) {
		return ret;
	}

	*cur = (*cur)->next;

	nd.str = NULL;
	ret = insert_policy_node_child(*cur, NODE_START_BLOCK, nd, lineno);
	if (ret != SELINT_SUCCESS) {
		*cur = (*cur)->prev;
		free_policy_node((*cur)->next);
		return ret;
	}

	*cur = (*cur)->first_child;

	return SELINT_SUCCESS;
}

static enum selint_error end_block(struct policy_node **cur,
                            enum node_flavor block_type)
{

	if ((*cur)->parent == NULL || (*cur)->parent->flavor != block_type) {
		return SELINT_NOT_IN_BLOCK;
	}

	*cur = (*cur)->parent;
	return SELINT_SUCCESS;
}

enum selint_error begin_define(struct policy_node **cur, unsigned int lineno)
{
	union node_data nd;
	nd.str = NULL;
	return begin_block(cur, NODE_DEFINE, nd, lineno);
}

enum selint_error end_define(struct policy_node **cur)
{

	return end_block(cur, NODE_DEFINE);
}

enum selint_error begin_optional_policy(struct policy_node **cur,
                                        unsigned int lineno)
{
	union node_data nd;
	nd.str = NULL;
	return begin_block(cur, NODE_OPTIONAL_POLICY, nd, lineno);
}

enum selint_error end_optional_policy(struct policy_node **cur)
{

	return end_block(cur, NODE_OPTIONAL_POLICY);
}

enum selint_error begin_optional_else(struct policy_node **cur,
                                      unsigned int lineno)
{
	union node_data nd;
	nd.str = NULL;
	return begin_block(cur, NODE_OPTIONAL_ELSE, nd, lineno);
}

enum selint_error end_optional_else(struct policy_node **cur)
{
	return end_block(cur, NODE_OPTIONAL_ELSE);
}

enum selint_error begin_boolean_policy(struct policy_node **cur,
                                       unsigned int lineno)
{
	struct cond_declaration_data *cd_data = xmalloc(sizeof(struct cond_declaration_data));
	cd_data->identifiers = NULL;

	union node_data nd;
	nd.cd_data = cd_data;

	return begin_block(cur, NODE_BOOLEAN_POLICY, nd, lineno);
}

enum selint_error end_boolean_policy(struct policy_node **cur)
{

	return end_block(cur, NODE_BOOLEAN_POLICY);
}

enum selint_error begin_tunable_policy(struct policy_node **cur,
                                       unsigned int lineno)
{
	struct cond_declaration_data *cd_data = xmalloc(sizeof(struct cond_declaration_data));
	cd_data->identifiers = NULL;

	union node_data nd;
	nd.cd_data = cd_data;

	return begin_block(cur, NODE_TUNABLE_POLICY, nd, lineno);
}

enum selint_error end_tunable_policy(struct policy_node **cur)
{

	return end_block(cur, NODE_TUNABLE_POLICY);
}

enum selint_error begin_interface_def(struct policy_node **cur,
                                      enum node_flavor flavor, const char *name,
                                      unsigned int lineno)
{

	switch (flavor) {
	case NODE_INTERFACE_DEF:
		break;
	case NODE_TEMP_DEF:
		insert_template_into_template_map(name);
		break;
	default:
		return SELINT_BAD_ARG;
	}

	insert_into_ifs_map(name, get_current_module_name());

	union node_data nd;
	nd.str = xstrdup(name);

	return begin_block(cur, flavor, nd, lineno);
}

enum selint_error end_interface_def(struct policy_node **cur)
{

	if (end_block(cur, NODE_INTERFACE_DEF) == SELINT_NOT_IN_BLOCK) {
		return end_block(cur, NODE_TEMP_DEF);
	} else {
		return SELINT_SUCCESS;
	}
}

enum selint_error begin_gen_require(struct policy_node **cur,
                                    unsigned int lineno)
{
	struct gen_require_data *data = (struct gen_require_data *)xmalloc(sizeof(struct gen_require_data));
	union node_data nd;
	nd.gr_data = data;
	return begin_block(cur, NODE_GEN_REQ, nd, lineno);
}

enum selint_error end_gen_require(struct policy_node **cur, unsigned char unquoted)
{

	if ((*cur)->parent && (*cur)->parent->flavor == NODE_GEN_REQ) {
		(*cur)->parent->data.gr_data->unquoted = unquoted;
	}

	return end_block(cur, NODE_GEN_REQ);
}

enum selint_error begin_require(struct policy_node **cur, unsigned int lineno)
{
	union node_data nd;
	nd.str = NULL;
	return begin_block(cur, NODE_REQUIRE, nd, lineno);
}

enum selint_error end_require(struct policy_node **cur)
{

	return end_block(cur, NODE_REQUIRE);
}

enum selint_error begin_ifdef(struct policy_node **cur, unsigned int lineno)
{
	union node_data nd;
	nd.str = NULL;
	return begin_block(cur, NODE_IFDEF, nd, lineno);
}

enum selint_error end_ifdef(struct policy_node **cur)
{
	return end_block(cur, NODE_IFDEF);
}

enum selint_error begin_ifelse(struct policy_node **cur, unsigned int lineno)
{
	union node_data nd;
	nd.str = NULL;
	return begin_block(cur, NODE_IFELSE, nd, lineno);
}

enum selint_error end_ifelse(struct policy_node **cur)
{
	return end_block(cur, NODE_IFELSE);
}

enum selint_error begin_m4_argument(struct policy_node **cur, unsigned int lineno)
{
	union node_data nd;
	nd.str = NULL;
	return begin_block(cur, NODE_M4_ARG, nd, lineno);
}

enum selint_error end_m4_argument(struct policy_node **cur)
{
	return end_block(cur, NODE_M4_ARG);
}

enum selint_error save_command(struct policy_node *cur, const char *comm)
{
	if (cur == NULL) {
		return SELINT_BAD_ARG;
	}
	if (comm == NULL) {
		return SELINT_SUCCESS;
	}
	while (*comm != 's' && *comm != '\0') {
		comm++;
	}
	if (0 != strncmp("selint-", comm, 7)) {
		return SELINT_PARSE_ERROR;
	}
	comm += strlen("selint-");
	if (0 == strncmp("disable:", comm, 8)) {
		cur->exceptions = xstrdup(comm + strlen("disable:"));
	} else {
		return SELINT_PARSE_ERROR;
	}

	return SELINT_SUCCESS;
}

enum selint_error save_identifier(struct policy_node *cur, char *identifier)
{
	if (cur == NULL || identifier == NULL) {
		free(identifier);
		return SELINT_BAD_ARG;
	}

	if (cur->flavor != NODE_TUNABLE_POLICY && cur->flavor != NODE_BOOLEAN_POLICY) {
		free(identifier);
		return SELINT_BAD_ARG;
	}

	cur->data.cd_data->identifiers = concat_string_lists(cur->data.cd_data->identifiers, sl_from_str_consume(identifier));

	return SELINT_SUCCESS;
}

static enum node_flavor attr_to_node_flavor(enum attr_flavor flavor)
{
	switch (flavor) {
	case ATTR_TYPE:
		return NODE_TYPE_ATTRIBUTE;
	case ATTR_ROLE:
		return NODE_ROLE_ATTRIBUTE;
	default:
		// Should never happen
		return NODE_ERROR;
	}
}

static enum selint_error insert_attribute(struct policy_node **cur, enum attr_flavor flavor, const char *type, struct string_list *attrs, unsigned int lineno)
{
	struct attribute_data *data = xcalloc(1, sizeof(struct attribute_data));
	if (!data) {
		return SELINT_OUT_OF_MEM;
	}
	union node_data nd;
	nd.at_data = data;

	data->type = xstrdup(type);
	data->attrs = attrs;
	data->flavor = flavor;

	enum selint_error ret = insert_policy_node_next(*cur, attr_to_node_flavor(flavor), nd, lineno);
	if (ret != SELINT_SUCCESS) {
		free(data);
		return ret;
	}

	*cur = (*cur)->next;

	if ((*cur)->parent &&
	    (*cur)->parent->flavor == NODE_INTERFACE_DEF &&
	    (is_transform_interface((*cur)->parent->data.str) ||
	     0 == strcmp(get_current_module_name(), "mls") ||
	     0 == strcmp(get_current_module_name(), "mcs"))) {
		mark_transform_if((*cur)->parent->data.str);
	}

	return SELINT_SUCCESS;
}

enum selint_error insert_type_attribute(struct policy_node **cur, const char *type, struct string_list *attrs, unsigned int lineno)
{
	return insert_attribute(cur, ATTR_TYPE, type, attrs, lineno);
}

enum selint_error insert_role_attribute(struct policy_node **cur, const char *role, struct string_list *attrs, unsigned int lineno)
{
	return insert_attribute(cur, ATTR_ROLE, role, attrs, lineno);
}

void cleanup_parsing(void)
{
	if (module_name) {
		free(module_name);
		module_name = NULL;
	}

	free_permmacros();

	free_all_maps();
}
