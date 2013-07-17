/*
  +----------------------------------------------------------------------+
  | PHP Version 5                                                        |
  +----------------------------------------------------------------------+
  | Copyright (c) 1997-2006 The PHP Group                                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.0 of the PHP license,       |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_0.txt.                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author: Sara Golemon <pollita@php.net>                               |
  +----------------------------------------------------------------------+
*/

/* $Id$ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "php_operator.h"

#ifndef ZEND_VM_KIND_CALL
/* ZEND_VM_KIND gets defined to this, but this doesn't get defined... go figure... */
#define ZEND_VM_KIND_CALL	1
#endif

#if defined(ZEND_ENGINE_2_1) && ZEND_VM_KIND != ZEND_VM_KIND_CALL
# error "Operator overload support requires CALL style Zend VM"
#endif

/* ***********
   * Helpers *
   *********** */

#ifndef EX_TMP_VAR
# define EX_TMP_VAR(ex, ofs)	((temp_variable *)((char*)(ex)->Ts + (ofs)))
#endif

static inline int php_operator_method(zval *result, zval *op1, const char *method, int method_len, zval *op2 TSRMLS_DC)
{
	zval *caller;
	int ret;

	ALLOC_INIT_ZVAL(caller);
	array_init(caller);

	Z_ADDREF_P(op1);
	add_index_zval(caller, 0, op1);
	add_index_stringl(caller, 1, (char*)method, method_len, 1);

	ret = call_user_function(EG(function_table), NULL, caller, result, op2 ? 1 : 0, op2 ? &op2 : NULL TSRMLS_CC);
	zval_ptr_dtor(&caller);

	return ret;
}

#ifdef ZEND_ENGINE_2_4
typedef znode_op operator_znode_op;
#else
typedef znode operator_znode_op;
#endif

static inline zval *php_operator_zval_ptr(zend_uchar optype, operator_znode_op *node_op, zend_free_op *should_free, zend_execute_data *execute_data, zend_bool silent TSRMLS_DC)
{
	should_free->var = NULL;

	switch (optype) {
		case IS_CONST:
#ifdef ZEND_ENGINE_2_4
			return &node_op->literal->constant;
#else
			return &(node_op->u.constant);
#endif
		case IS_VAR:
			return EX_TMP_VAR(execute_data, PHP_OPERATOR_OP_U(*node_op).var)->var.ptr;
		case IS_TMP_VAR:
			return (should_free->var = &EX_TMP_VAR(execute_data, PHP_OPERATOR_OP_U(*node_op).var)->tmp_var);
#ifdef ZEND_ENGINE_2_1
		case IS_CV:
		{
			zval ***ret = EX_CV_NUM(execute_data, PHP_OPERATOR_OP_U(*node_op).var);

			if (!*ret) {
				zend_compiled_variable *cv = &EG(active_op_array)->vars[PHP_OPERATOR_OP_U(*node_op).var];
				if (zend_hash_quick_find(EG(active_symbol_table), cv->name, cv->name_len+1, cv->hash_value, (void **)ret)==FAILURE) {
					if (!silent) {
						zend_error(E_NOTICE, "Undefined variable: %s", cv->name);
					}
					return &EG(uninitialized_zval);
				}
			}
			return **ret;
		}
#endif
		case IS_UNUSED:
		default:
			return NULL;
	}
}

static inline zval **php_operator_zval_ptr_ptr(zend_uchar optype, operator_znode_op *node_op, zend_execute_data *execute_data, zend_bool silent TSRMLS_DC)
{
	switch (optype) {
		case IS_VAR:
			return EX_TMP_VAR(execute_data, PHP_OPERATOR_OP_U(*node_op).var)->var.ptr_ptr;
#ifdef ZEND_ENGINE_2_1
		case IS_CV:
		{
			zval ***ret = EX_CV_NUM(execute_data, PHP_OPERATOR_OP_U(*node_op).var);

			if (!*ret) {
				zend_compiled_variable *cv = &EG(active_op_array)->vars[PHP_OPERATOR_OP_U(*node_op).var];
				if (zend_hash_quick_find(EG(active_symbol_table), cv->name, cv->name_len+1, cv->hash_value, (void **)ret)==FAILURE) {
					if (!silent) {
						zend_error(E_NOTICE, "Undefined variable: %s", cv->name);
					}
					return &EG(uninitialized_zval_ptr);
				}
			}
			return *ret;
		}
#endif
		case IS_CONST:
		case IS_TMP_VAR:
		case IS_UNUSED:
		default:
			return NULL;
	}	
}

static inline zval *php_operator_get_result_ptr(zend_op *opline, zend_execute_data *execute_data)
{
	zval *tmp;

	if (PHP_OPERATOR_OP_TYPE(opline->result) == IS_TMP_VAR) {
			return &EX_TMP_VAR(execute_data, PHP_OPERATOR_OP_U(opline->result).var)->tmp_var;
	}

	ALLOC_INIT_ZVAL(tmp);
	return tmp;
}

static inline void php_operator_set_result_ptr(zval *result, zend_op *opline, zend_execute_data *execute_data)
{
	switch (PHP_OPERATOR_OP_TYPE(opline->result)) {
		case IS_TMP_VAR:
			/* Nothing to do */
			return;
		case IS_VAR:
			EX_TMP_VAR(execute_data, PHP_OPERATOR_OP_U(opline->result).var)->var.ptr = result;
			EX_TMP_VAR(execute_data, PHP_OPERATOR_OP_U(opline->result).var)->var.ptr_ptr =
                                         &EX_TMP_VAR(execute_data, PHP_OPERATOR_OP_U(opline->result).var)->var.ptr;
			return;
		default:
			zval_ptr_dtor(&result);
	}
}

#ifdef ZEND_ENGINE_2_1
static inline int _php_operator_decode(zend_op *opline)
{
	int ret = opline->opcode * 25;
	switch (PHP_OPERATOR_OP_TYPE(opline->op1)) {
		case IS_CONST:					break;
		case IS_TMP_VAR:	ret += 5;	break;
		case IS_VAR:		ret += 10;	break;
		case IS_UNUSED:		ret += 15;	break;
		case IS_CV:			ret += 20;	break;
	}
	switch (PHP_OPERATOR_OP_TYPE(opline->op2)) {
		case IS_CONST:					break;
		case IS_TMP_VAR:	ret += 1;	break;
		case IS_VAR:		ret += 2;	break;
		case IS_UNUSED:		ret += 3;	break;
		case IS_CV:			ret += 4;	break;
	}
	return ret;
}

#if defined(ZEND_ENGINE_2_5)
# define PHP_OPERATOR_OPCODE_COUNT		163
#elif defined(ZEND_ENGINE_2_4)
# define PHP_OPERATOR_OPCODE_COUNT		158
#elif defined(ZEND_ENGINE_2_3)
# define PHP_OPERATOR_OPCODE_COUNT		153
#else /* PHP 5.1 and 5.2 */
# define PHP_OPERATOR_OPCODE_COUNT		150
#endif

#define PHP_OPERATOR_OPHANDLER_COUNT				((25 * (PHP_OPERATOR_OPCODE_COUNT + 1)) + 1)
#define PHP_OPERATOR_REPLACE_OPCODE(opname)			{ int i; for(i = 5; i < 25; i++) if (php_operator_opcode_handlers[(opname*25) + i]) php_operator_opcode_handlers[(opname*25) + i] = php_operator_op_##opname; }
#define PHP_OPERATOR_REPLACE_ALL_OPCODE(opname)		{ int i; for(i = 0; i < 25; i++) if (php_operator_opcode_handlers[(opname*25) + i]) php_operator_opcode_handlers[(opname*25) + i] = php_operator_op_##opname; }
#define PHP_OPERATOR_DECODE(opline)					_php_operator_decode(opline)
#define PHP_OPERATOR_GET_OPLINE						zend_op *opline = (execute_data->opline);
#else
#define PHP_OPERATOR_OPCODE_COUNT				512
#define PHP_OPERATOR_OPHANDLER_COUNT				PHP_OPERATOR_OPCODE_COUNT
#define PHP_OPERATOR_REPLACE_OPCODE(opname)			zend_opcode_handlers[opname] = php_operator_op_##opname
#define PHP_OPERATOR_REPLACE_ALL_OPCODE(opname)		zend_opcode_handlers[opname] = php_operator_op_##opname
#define PHP_OPERATOR_DECODE(opline)					(opline->opcode)
#define PHP_OPERATOR_GET_OPLINE							
#endif

static opcode_handler_t *php_operator_original_opcode_handlers;
static opcode_handler_t php_operator_opcode_handlers[PHP_OPERATOR_OPHANDLER_COUNT];

#ifndef ZEND_FASTCALL
#define ZEND_FASTCALL
#endif

#ifndef Z_ADDREF_P
# define Z_ADDREF_P(pzv) ((pzv)->refcount++)
#endif

/* *******************
   * Op Replacements *
   ******************* */

#define PHP_OPERATOR_BINARY_OP(opname,methodname) static int ZEND_FASTCALL php_operator_op_##opname (ZEND_OPCODE_HANDLER_ARGS) { return _php_operator_binary_op(ZEND_OPCODE_HANDLER_ARGS_PASSTHRU, methodname, sizeof(methodname) - 1); }
static inline int _php_operator_binary_op(ZEND_OPCODE_HANDLER_ARGS, const char *methodname, int methodname_len)
{
	PHP_OPERATOR_GET_OPLINE
	zend_free_op free_op1, free_op2;
	zval *result;
	zval *op1 = php_operator_zval_ptr(PHP_OPERATOR_OP_TYPE(opline->op1), &(opline->op1), &free_op1, execute_data, (opline->opcode == ZEND_ASSIGN) TSRMLS_CC);
	zval *op2 = php_operator_zval_ptr(PHP_OPERATOR_OP_TYPE(opline->op2), &(opline->op2), &free_op2, execute_data, 0 TSRMLS_CC);

#ifdef ZEND_HAVE_DO_BINARY_COMPARE_OP
	if (opline->extended_value &&
		opline->opcode == ZEND_IS_SMALLER) {
		zval *swap = op1; op1 = op2; op2 = swap;

		methodname = "__is_greater";
		methodname_len = sizeof("__is_greater") - 1;
	} else if (opline->extended_value &&
		opline->opcode == ZEND_IS_SMALLER_OR_EQUAL) {
		zval *swap = op1; op1 = op2; op2 = swap;

		methodname = "__is_greater_or_equal";
		methodname_len = sizeof("__is_greater_or_equal") - 1;
	}
#endif

	if (!op1 || op1->type != IS_OBJECT ||
		!zend_hash_exists(&Z_OBJCE_P(op1)->function_table, (char*)methodname, methodname_len + 1)) {
		/* Rely on primary handler */
		return php_operator_original_opcode_handlers[PHP_OPERATOR_DECODE(opline)](ZEND_OPCODE_HANDLER_ARGS_PASSTHRU);
	}

	result = php_operator_get_result_ptr(opline, execute_data);
	if (php_operator_method(result, op1, methodname, methodname_len, op2 TSRMLS_CC) == FAILURE) {
		/* Fallback on original handler */
		if (PHP_OPERATOR_OP_TYPE(opline->result) != IS_TMP_VAR) zval_ptr_dtor(&result);
		return php_operator_original_opcode_handlers[PHP_OPERATOR_DECODE(opline)](ZEND_OPCODE_HANDLER_ARGS_PASSTHRU);
	}

	php_operator_set_result_ptr(result, opline, execute_data);
	if (free_op1.var) zval_dtor(free_op1.var);
	if (free_op2.var) zval_dtor(free_op2.var);
	execute_data->opline++;
	return 0;
}

#define PHP_OPERATOR_UNARY_OP(opname,methodname) static int ZEND_FASTCALL php_operator_op_##opname (ZEND_OPCODE_HANDLER_ARGS) { return _php_operator_unary_op(ZEND_OPCODE_HANDLER_ARGS_PASSTHRU, methodname, sizeof(methodname) - 1); }
static inline int _php_operator_unary_op(ZEND_OPCODE_HANDLER_ARGS, const char *methodname, int methodname_len)
{
	PHP_OPERATOR_GET_OPLINE
	zend_free_op free_op1;
	zval *result;
	zval *op1 = php_operator_zval_ptr(PHP_OPERATOR_OP_TYPE(opline->op1), &(opline->op1), &free_op1, execute_data, 0 TSRMLS_CC);

	if (!op1 ||
		op1->type != IS_OBJECT ||
		!zend_hash_exists(&Z_OBJCE_P(op1)->function_table, (char*)methodname, methodname_len + 1)) {
		/* Rely on primary handler */
		return php_operator_original_opcode_handlers[PHP_OPERATOR_DECODE(opline)](ZEND_OPCODE_HANDLER_ARGS_PASSTHRU);
	}

	result = php_operator_get_result_ptr(opline, execute_data);
	if (php_operator_method(result, op1, methodname, methodname_len, NULL TSRMLS_CC) == FAILURE) {
		/* Fallback on original handler */
		if (PHP_OPERATOR_OP_TYPE(opline->result) != IS_TMP_VAR) zval_ptr_dtor(&result);
		return php_operator_original_opcode_handlers[PHP_OPERATOR_DECODE(opline)](ZEND_OPCODE_HANDLER_ARGS_PASSTHRU);
	}

	php_operator_set_result_ptr(result, opline, execute_data);
	if (free_op1.var) zval_dtor(free_op1.var);
	execute_data->opline++;
	return 0;
}

#ifdef ZEND_ENGINE_2_5
# define BP_VAR_RW_55_CC , BP_VAR_RW
#else
# define BP_VAR_RW_55_CC
#endif

#define PHP_OPERATOR_BINARY_ASSIGN_OP(opname,methodname) static int ZEND_FASTCALL php_operator_op_##opname (ZEND_OPCODE_HANDLER_ARGS) { return _php_operator_binary_assign_op(ZEND_OPCODE_HANDLER_ARGS_PASSTHRU, methodname, sizeof(methodname) - 1); }
static int _php_operator_binary_assign_op(ZEND_OPCODE_HANDLER_ARGS, const char *methodname, int methodname_len)
{
	PHP_OPERATOR_GET_OPLINE
	zend_free_op free_value, free_prop, free_obj;
	zval *var = NULL, *value, *result;
	zend_bool increment_opline = 0;

	free_prop.var = free_obj.var = NULL;
	switch (opline->extended_value) {
		case ZEND_ASSIGN_OBJ:
		case ZEND_ASSIGN_DIM:
		{
			zend_op *opdata = opline + 1;
			zval *object = php_operator_zval_ptr(PHP_OPERATOR_OP_TYPE(opline->op1), &(opline->op1), &free_obj, execute_data, 0 TSRMLS_CC);
			zval *prop = php_operator_zval_ptr(PHP_OPERATOR_OP_TYPE(opline->op2), &(opline->op2), &free_prop, execute_data, (opline->opcode == ZEND_ASSIGN) TSRMLS_CC);

			if (!object || Z_TYPE_P(object) != IS_OBJECT || !prop) {
				/* Let orignal handler throw error */
				return php_operator_original_opcode_handlers[PHP_OPERATOR_DECODE(opline)](ZEND_OPCODE_HANDLER_ARGS_PASSTHRU);
			}

			increment_opline = 1;
			value = php_operator_zval_ptr(PHP_OPERATOR_OP_TYPE(opdata->op1), &(opdata->op1), &free_value, execute_data, 0 TSRMLS_CC);
			if (!value) {
				/* Shouldn't happen */
				return php_operator_original_opcode_handlers[PHP_OPERATOR_DECODE(opline)](ZEND_OPCODE_HANDLER_ARGS_PASSTHRU);
			}

			/* Plan A */
			if (opline->extended_value == ZEND_ASSIGN_OBJ &&
				Z_OBJ_HT_P(object)->get_property_ptr_ptr) {
				zval **varpp = Z_OBJ_HT_P(object)->get_property_ptr_ptr(object, prop BP_VAR_RW_55_CC PHP_OPERATOR_LITERAL_NULL_CC TSRMLS_CC);
				if (varpp) {
					var = *varpp;
					break;
				}
			}

			/* Plan B */
			if (opline->extended_value == ZEND_ASSIGN_OBJ &&
				Z_OBJ_HT_P(object)->read_property) {
				var = Z_OBJ_HT_P(object)->read_property(object, prop, BP_VAR_RW PHP_OPERATOR_LITERAL_NULL_CC TSRMLS_CC);
			} else if (opline->extended_value == ZEND_ASSIGN_DIM &&
				Z_OBJ_HT_P(object)->read_dimension) {
				var = Z_OBJ_HT_P(object)->read_dimension(object, prop, BP_VAR_RW TSRMLS_CC);
			}

			break;
		}
		default:
			var = php_operator_zval_ptr(PHP_OPERATOR_OP_TYPE(opline->op1), &(opline->op1), &free_obj, execute_data, (opline->opcode == ZEND_ASSIGN) TSRMLS_CC);
			value = php_operator_zval_ptr(PHP_OPERATOR_OP_TYPE(opline->op2), &(opline->op2), &free_value, execute_data, 0 TSRMLS_CC);
	}

	if (!var || Z_TYPE_P(var) != IS_OBJECT ||
		!zend_hash_exists(&Z_OBJCE_P(var)->function_table, (char*)methodname, methodname_len + 1)) {
		/* Rely on primary handler */
		return php_operator_original_opcode_handlers[PHP_OPERATOR_DECODE(opline)](ZEND_OPCODE_HANDLER_ARGS_PASSTHRU);
	}

	result = php_operator_get_result_ptr(opline, execute_data);
	if (php_operator_method(result, var, methodname, methodname_len, value TSRMLS_CC) == FAILURE) {
		/* Fallback on original handler */
		if (PHP_OPERATOR_OP_TYPE(opline->result) != IS_TMP_VAR) zval_ptr_dtor(&result);
		return php_operator_original_opcode_handlers[PHP_OPERATOR_DECODE(opline)](ZEND_OPCODE_HANDLER_ARGS_PASSTHRU);
	}

	if (free_prop.var) zval_dtor(free_prop.var);
	if (free_value.var) zval_dtor(free_value.var);
	php_operator_set_result_ptr(result, opline, execute_data);
	execute_data->opline += increment_opline ? 2 : 1;
	return 0;
}

#define PHP_OPERATOR_UNARY_ASSIGN_OP(opname,methodname)	static int ZEND_FASTCALL php_operator_op_##opname	(ZEND_OPCODE_HANDLER_ARGS) { return _php_operator_unary_assign_op(ZEND_OPCODE_HANDLER_ARGS_PASSTHRU, methodname, sizeof(methodname) - 1); }
static inline int _php_operator_unary_assign_op(ZEND_OPCODE_HANDLER_ARGS, const char *methodname, int methodname_len)
{
	PHP_OPERATOR_GET_OPLINE
	zval *result;
	zval **op1 = php_operator_zval_ptr_ptr(PHP_OPERATOR_OP_TYPE(opline->op1), &(opline->op1), execute_data, 0 TSRMLS_CC);

	if (!op1 || Z_TYPE_PP(op1) != IS_OBJECT ||
		!zend_hash_exists(&Z_OBJCE_PP(op1)->function_table, (char*)methodname, methodname_len + 1)) {
		/* Rely on primary handler */
		return php_operator_original_opcode_handlers[PHP_OPERATOR_DECODE(opline)](ZEND_OPCODE_HANDLER_ARGS_PASSTHRU);
	}

	result = php_operator_get_result_ptr(opline, execute_data);
	if (php_operator_method(result, *op1, methodname, methodname_len, NULL TSRMLS_CC) == FAILURE) {
		/* Fallback on original handler */
		if (PHP_OPERATOR_OP_TYPE(opline->result) != IS_TMP_VAR) zval_ptr_dtor(&result);
		return php_operator_original_opcode_handlers[PHP_OPERATOR_DECODE(opline)](ZEND_OPCODE_HANDLER_ARGS_PASSTHRU);
	}

	php_operator_set_result_ptr(result, opline, execute_data);
	execute_data->opline++;
	return 0;
}

#define PHP_OPERATOR_UNARY_ASSIGN_OBJ_OP(opname,methodname) static int ZEND_FASTCALL php_operator_op_##opname (ZEND_OPCODE_HANDLER_ARGS) { return _php_operator_unary_assign_obj_op(ZEND_OPCODE_HANDLER_ARGS_PASSTHRU, methodname, sizeof(methodname) - 1); }
static inline int _php_operator_unary_assign_obj_op(ZEND_OPCODE_HANDLER_ARGS, const char *methodname, int methodname_len)
{
	PHP_OPERATOR_GET_OPLINE
	zend_free_op free_obj, free_prop;
	zval *result;
	zval *obj = php_operator_zval_ptr(PHP_OPERATOR_OP_TYPE(opline->op1), &(opline->op1), &free_obj, execute_data, 0 TSRMLS_CC);
	zval *prop = php_operator_zval_ptr(PHP_OPERATOR_OP_TYPE(opline->op2), &(opline->op2), &free_prop, execute_data, 0 TSRMLS_CC);
	zval *var = NULL;

	if (!obj || Z_TYPE_P(obj) != IS_OBJECT || !prop) {
		/* Rely on primary handler */
		return php_operator_original_opcode_handlers[PHP_OPERATOR_DECODE(opline)](ZEND_OPCODE_HANDLER_ARGS_PASSTHRU);
	}

	if (Z_OBJ_HT_P(obj)->get_property_ptr_ptr) {
		zval **varpp = Z_OBJ_HT_P(obj)->get_property_ptr_ptr(obj, prop BP_VAR_RW_55_CC PHP_OPERATOR_LITERAL_NULL_CC TSRMLS_CC);
		if (varpp) {
			var = *varpp;
		}
	}
	if (!var && Z_OBJ_HT_P(obj)->read_property) {
		var = Z_OBJ_HT_P(obj)->read_property(obj, prop, BP_VAR_RW PHP_OPERATOR_LITERAL_NULL_CC TSRMLS_CC);
	}

	if (!var || Z_TYPE_P(var) != IS_OBJECT ||
		!zend_hash_exists(&Z_OBJCE_P(var)->function_table, (char*)methodname, methodname_len + 1)) {
		/* Rely on primary handler */
		return php_operator_original_opcode_handlers[PHP_OPERATOR_DECODE(opline)](ZEND_OPCODE_HANDLER_ARGS_PASSTHRU);
	}

	result = php_operator_get_result_ptr(opline, execute_data);
	if (php_operator_method(result, var, methodname, methodname_len, NULL TSRMLS_CC) == FAILURE) {
		/* Fallback on original handler */
		if (PHP_OPERATOR_OP_TYPE(opline->result) != IS_TMP_VAR) zval_ptr_dtor(&result);
		return php_operator_original_opcode_handlers[PHP_OPERATOR_DECODE(opline)](ZEND_OPCODE_HANDLER_ARGS_PASSTHRU);
	}

	if (free_obj.var) { zval_dtor(free_obj.var); }
	if (free_prop.var) { zval_dtor(free_prop.var); }
	php_operator_set_result_ptr(result, opline, execute_data);
	execute_data->opline++;
	return 0;
}

PHP_OPERATOR_BINARY_OP(ZEND_ADD,					"__add")
PHP_OPERATOR_BINARY_OP(ZEND_SUB,					"__sub")
PHP_OPERATOR_BINARY_OP(ZEND_MUL,					"__mul")
PHP_OPERATOR_BINARY_OP(ZEND_DIV,					"__div")
PHP_OPERATOR_BINARY_OP(ZEND_MOD,					"__mod")
PHP_OPERATOR_BINARY_OP(ZEND_SL,						"__sl")
PHP_OPERATOR_BINARY_OP(ZEND_SR,						"__sr")
PHP_OPERATOR_BINARY_OP(ZEND_CONCAT,					"__concat")
PHP_OPERATOR_BINARY_OP(ZEND_BW_OR,					"__bw_or")
PHP_OPERATOR_BINARY_OP(ZEND_BW_AND,					"__bw_and")
PHP_OPERATOR_BINARY_OP(ZEND_BW_XOR,					"__bw_xor")

PHP_OPERATOR_BINARY_OP(ZEND_IS_IDENTICAL,			"__is_identical")
PHP_OPERATOR_BINARY_OP(ZEND_IS_NOT_IDENTICAL,		"__is_not_identical")
PHP_OPERATOR_BINARY_OP(ZEND_IS_EQUAL,				"__is_equal")
PHP_OPERATOR_BINARY_OP(ZEND_IS_NOT_EQUAL,			"__is_not_equal")

PHP_OPERATOR_BINARY_OP(ZEND_IS_SMALLER,				"__is_smaller") /* includes __is_greater when patch applied */
PHP_OPERATOR_BINARY_OP(ZEND_IS_SMALLER_OR_EQUAL,	"__is_smaller_or_equal") /* includes __is_greater_or_equal ... */

PHP_OPERATOR_UNARY_OP(ZEND_BW_NOT,					"__bw_not")

PHP_OPERATOR_BINARY_ASSIGN_OP(ZEND_ASSIGN,		"__assign")
PHP_OPERATOR_BINARY_ASSIGN_OP(ZEND_ASSIGN_ADD,		"__assign_add")
PHP_OPERATOR_BINARY_ASSIGN_OP(ZEND_ASSIGN_SUB,		"__assign_sub")
PHP_OPERATOR_BINARY_ASSIGN_OP(ZEND_ASSIGN_MUL,		"__assign_mul")
PHP_OPERATOR_BINARY_ASSIGN_OP(ZEND_ASSIGN_DIV,		"__assign_div")
PHP_OPERATOR_BINARY_ASSIGN_OP(ZEND_ASSIGN_MOD,		"__assign_mod")
PHP_OPERATOR_BINARY_ASSIGN_OP(ZEND_ASSIGN_SL,		"__assign_sl")
PHP_OPERATOR_BINARY_ASSIGN_OP(ZEND_ASSIGN_SR,		"__assign_sr")
PHP_OPERATOR_BINARY_ASSIGN_OP(ZEND_ASSIGN_CONCAT,	"__assign_concat")
PHP_OPERATOR_BINARY_ASSIGN_OP(ZEND_ASSIGN_BW_OR,	"__assign_bw_or")
PHP_OPERATOR_BINARY_ASSIGN_OP(ZEND_ASSIGN_BW_AND,	"__assign_bw_and")
PHP_OPERATOR_BINARY_ASSIGN_OP(ZEND_ASSIGN_BW_XOR,	"__assign_bw_xor")

PHP_OPERATOR_UNARY_ASSIGN_OP(ZEND_PRE_INC,			"__pre_inc")
PHP_OPERATOR_UNARY_ASSIGN_OP(ZEND_PRE_DEC,			"__pre_dec")
PHP_OPERATOR_UNARY_ASSIGN_OP(ZEND_POST_INC,			"__post_inc")
PHP_OPERATOR_UNARY_ASSIGN_OP(ZEND_POST_DEC,			"__post_dec")

PHP_OPERATOR_UNARY_ASSIGN_OBJ_OP(ZEND_PRE_INC_OBJ,	"__pre_inc")
PHP_OPERATOR_UNARY_ASSIGN_OBJ_OP(ZEND_PRE_DEC_OBJ,	"__pre_dec")
PHP_OPERATOR_UNARY_ASSIGN_OBJ_OP(ZEND_POST_INC_OBJ,	"__post_inc")
PHP_OPERATOR_UNARY_ASSIGN_OBJ_OP(ZEND_POST_DEC_OBJ,	"__post_dec")

/* ***********************
   * Module Housekeeping *
   *********************** */

PHP_MINIT_FUNCTION(operator)
{
	memcpy(php_operator_opcode_handlers, zend_opcode_handlers, sizeof(php_operator_opcode_handlers));

#if PHP_MAJOR_VERSION > 5 || PHP_MINOR_VERSION > 0
	php_operator_original_opcode_handlers = zend_opcode_handlers;
	zend_opcode_handlers = php_operator_opcode_handlers;
#else
	php_operator_original_opcode_handlers = php_operator_opcode_handlers;
#endif

	/* Binaries */
	PHP_OPERATOR_REPLACE_OPCODE(ZEND_ADD);
	PHP_OPERATOR_REPLACE_OPCODE(ZEND_SUB);
	PHP_OPERATOR_REPLACE_OPCODE(ZEND_MUL);
	PHP_OPERATOR_REPLACE_OPCODE(ZEND_DIV);
	PHP_OPERATOR_REPLACE_OPCODE(ZEND_MOD);
	PHP_OPERATOR_REPLACE_OPCODE(ZEND_SL);
	PHP_OPERATOR_REPLACE_OPCODE(ZEND_SR);
	PHP_OPERATOR_REPLACE_OPCODE(ZEND_CONCAT);
	PHP_OPERATOR_REPLACE_OPCODE(ZEND_BW_OR);
	PHP_OPERATOR_REPLACE_OPCODE(ZEND_BW_AND);
	PHP_OPERATOR_REPLACE_OPCODE(ZEND_BW_XOR);

	/* Comparators (Binaries in disguise) */
	PHP_OPERATOR_REPLACE_OPCODE(ZEND_IS_IDENTICAL);
	PHP_OPERATOR_REPLACE_OPCODE(ZEND_IS_NOT_IDENTICAL);
	PHP_OPERATOR_REPLACE_OPCODE(ZEND_IS_EQUAL);
	PHP_OPERATOR_REPLACE_OPCODE(ZEND_IS_NOT_EQUAL);

#ifdef ZEND_HAVE_DO_BINARY_COMPARE_OP
/* __is_greater and __is_greater_or_equal support requires patching parser: compare-greater-VERSION.diff */
	REGISTER_LONG_CONSTANT("OPERATOR_COMPARE_PATCH",	1,		CONST_CS | CONST_PERSISTENT);
	PHP_OPERATOR_REPLACE_ALL_OPCODE(ZEND_IS_SMALLER);
	PHP_OPERATOR_REPLACE_ALL_OPCODE(ZEND_IS_SMALLER_OR_EQUAL);
#else
	REGISTER_LONG_CONSTANT("OPERATOR_COMPARE_PATCH",	0,		CONST_CS | CONST_PERSISTENT);
	PHP_OPERATOR_REPLACE_OPCODE(ZEND_IS_SMALLER);
	PHP_OPERATOR_REPLACE_OPCODE(ZEND_IS_SMALLER_OR_EQUAL);
#endif

	/* Unaries */
	PHP_OPERATOR_REPLACE_OPCODE(ZEND_BW_NOT);

	/* Binary Assign */
	PHP_OPERATOR_REPLACE_OPCODE(ZEND_ASSIGN);
	PHP_OPERATOR_REPLACE_OPCODE(ZEND_ASSIGN_ADD);
	PHP_OPERATOR_REPLACE_OPCODE(ZEND_ASSIGN_SUB);
	PHP_OPERATOR_REPLACE_OPCODE(ZEND_ASSIGN_MUL);
	PHP_OPERATOR_REPLACE_OPCODE(ZEND_ASSIGN_DIV);
	PHP_OPERATOR_REPLACE_OPCODE(ZEND_ASSIGN_MOD);
	PHP_OPERATOR_REPLACE_OPCODE(ZEND_ASSIGN_SL);
	PHP_OPERATOR_REPLACE_OPCODE(ZEND_ASSIGN_SR);
	PHP_OPERATOR_REPLACE_OPCODE(ZEND_ASSIGN_CONCAT);
	PHP_OPERATOR_REPLACE_OPCODE(ZEND_ASSIGN_BW_OR);
	PHP_OPERATOR_REPLACE_OPCODE(ZEND_ASSIGN_BW_AND);
	PHP_OPERATOR_REPLACE_OPCODE(ZEND_ASSIGN_BW_XOR);

	/* Unary Assign */
	PHP_OPERATOR_REPLACE_OPCODE(ZEND_PRE_INC);
	PHP_OPERATOR_REPLACE_OPCODE(ZEND_PRE_DEC);
	PHP_OPERATOR_REPLACE_OPCODE(ZEND_POST_INC);
	PHP_OPERATOR_REPLACE_OPCODE(ZEND_POST_DEC);

	/* Unary Assign Obj */
	PHP_OPERATOR_REPLACE_OPCODE(ZEND_PRE_INC_OBJ);
	PHP_OPERATOR_REPLACE_OPCODE(ZEND_PRE_DEC_OBJ);
	PHP_OPERATOR_REPLACE_OPCODE(ZEND_POST_INC_OBJ);
	PHP_OPERATOR_REPLACE_OPCODE(ZEND_POST_DEC_OBJ);

	return SUCCESS;
}


PHP_MSHUTDOWN_FUNCTION(operator)
{
#if PHP_MAJOR_VERSION > 5 || PHP_MINOR_VERSION > 0
	zend_opcode_handlers = php_operator_original_opcode_handlers;
#else
	memcpy(zend_opcode_handlers, php_operator_original_opcode_handlers, sizeof(php_operator_opcode_handlers));
#endif

	return SUCCESS;
}

PHP_MINFO_FUNCTION(operator)
{
	php_info_print_table_start();
	php_info_print_table_header(2, "operator overloading support", "+ - * / % << >> . | & ^ ~ ! ++ -- "
									"+= -= *= /= %= <<= >>= .= |= &= ^= ~= === !== == != < <= "
#ifdef ZEND_HAVE_DO_BINARY_COMPARE_OP
									"> >= "
#endif
									);
	php_info_print_table_row(2, "version", PHP_OPERATOR_VERSION);
	php_info_print_table_end();
}

zend_module_entry operator_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
	STANDARD_MODULE_HEADER,
#endif
	PHP_OPERATOR_EXTNAME,
	NULL,
	PHP_MINIT(operator),
	PHP_MSHUTDOWN(operator),
	NULL, /* RINIT */
	NULL, /* RSHUTDOWN */
	PHP_MINFO(operator),
#if ZEND_MODULE_API_NO >= 20010901
	PHP_OPERATOR_VERSION,
#endif
	STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_OPERATOR
ZEND_GET_MODULE(operator)
#endif

