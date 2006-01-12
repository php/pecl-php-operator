/*
  +----------------------------------------------------------------------+
  | PHP Version 5                                                        |
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

#if (PHP_MAJOR_VERSION > 5 || PHP_MINOR_VERSION > 0) && ZEND_VM_KIND != ZEND_VM_KIND_CALL
# error "Operator overload support requires CALL style Zend VM"
#endif

/* ***********
   * Helpers *
   *********** */

static int php_operator_method(zval *result, zval *op1, const char *method, int method_len, zval *op2 TSRMLS_DC)
{
	zval *caller;
	int ret;

	ALLOC_INIT_ZVAL(caller);
	array_init(caller);

	ZVAL_ADDREF(op1);
	add_index_zval(caller, 0, op1);
	add_index_stringl(caller, 1, (char*)method, method_len, 1);

	ret = call_user_function(EG(function_table), NULL, caller, result, op2 ? 1 : 0, op2 ? &op2 : NULL TSRMLS_CC);
	zval_ptr_dtor(&caller);

	return ret;
}

#define PHP_OPERATOR_EX_T(offset)	(*(temp_variable *)((char*)execute_data->Ts + offset))
#define PHP_OPERATOR_RESULT			&PHP_OPERATOR_EX_T(opline->result.u.var).tmp_var

static zval *php_operator_zval_ptr(znode *node, zend_free_op *should_free, zend_execute_data *execute_data TSRMLS_DC)
{
	should_free->var = NULL;

	switch (node->op_type) {
		case IS_CONST:
			return &(node->u.constant);
		case IS_VAR:
			return PHP_OPERATOR_EX_T(node->u.var).var.ptr;
		case IS_TMP_VAR:
			return (should_free->var = &PHP_OPERATOR_EX_T(node->u.var).tmp_var);
#if PHP_MAJOR_VERSION > 5 || PHP_MINOR_VERSION > 0
		case IS_CV:
		{
			zval ***ret = &execute_data->CVs[node->u.var];

			if (!*ret) {
				zend_compiled_variable *cv = &EG(active_op_array)->vars[node->u.var];
				if (zend_hash_quick_find(EG(active_symbol_table), cv->name, cv->name_len+1, cv->hash_value, (void **)ret)==FAILURE) {
					zend_error(E_NOTICE, "Undefined variable: %s", cv->name);
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

#if PHP_MAJOR_VERSION > 5 || PHP_MINOR_VERSION > 0
static inline int _php_operator_decode(zend_uchar opcode, zend_op *opline)
{
	int ret = opcode * 25;
	switch (opline->op1.op_type) {
		case IS_CONST:					break;
		case IS_TMP_VAR:	ret += 5;	break;
		case IS_VAR:		ret += 10;	break;
		case IS_UNUSED:		ret += 15;	break;
		case IS_CV:			ret += 20;	break;
	}
	switch (opline->op2.op_type) {
		case IS_CONST:					break;
		case IS_TMP_VAR:	ret += 1;	break;
		case IS_VAR:		ret += 2;	break;
		case IS_UNUSED:		ret += 3;	break;
		case IS_CV:			ret += 4;	break;
	}
	return ret;
}
#define PHP_OPERATOR_OPHANDLER_COUNT				((25 * 151) + 1)
#define PHP_OPERATOR_REPLACE_OPCODE(opname)			{ int i; for(i = 5; i < 25; i++) if (php_operator_opcode_handlers[(opname*25) + i]) php_operator_opcode_handlers[(opname*25) + i] = php_operator_op_##opname; }
#define PHP_OPERATOR_DECODE(opcode,opline)			_php_operator_decode(opcode,opline)
#define PHP_OPERATOR_GET_OPLINE						zend_op *opline = (execute_data->opline);
#else
#define PHP_OPERATOR_OPHANDLER_COUNT				512
#define PHP_OPERATOR_REPLACE_OPCODE(opname)			zend_opcode_handlers[opname] = php_operator_op_##opname
#define PHP_OPERATOR_DECODE(opcode,opline)			(opcode)
#define PHP_OPERATOR_GET_OPLINE							
#endif

static opcode_handler_t *php_operator_original_opcode_handlers;
static opcode_handler_t php_operator_opcode_handlers[PHP_OPERATOR_OPHANDLER_COUNT];

/* *******************
   * Op Replacements *
   ******************* */

#define PHP_OPERATOR_BINARY_OP(opname,methodname)		\
static int php_operator_op_##opname (ZEND_OPCODE_HANDLER_ARGS) \
{ \
	PHP_OPERATOR_GET_OPLINE \
	zend_free_op free_op1, free_op2; \
	zval *op1 = php_operator_zval_ptr(&(opline->op1), &free_op1, execute_data TSRMLS_CC); \
	zval *op2 = php_operator_zval_ptr(&(opline->op2), &free_op2, execute_data TSRMLS_CC); \
\
	if (opline->op1.op_type == IS_CONST || \
		op1->type != IS_OBJECT || \
		!zend_hash_exists(&Z_OBJCE_P(op1)->function_table, methodname, sizeof(methodname))) { \
		/* Rely on primary handler */ \
		return php_operator_original_opcode_handlers[PHP_OPERATOR_DECODE(opname,opline)](ZEND_OPCODE_HANDLER_ARGS_PASSTHRU); \
	} \
\
	if (php_operator_method(PHP_OPERATOR_RESULT, op1, methodname, sizeof(methodname) - 1, op2 TSRMLS_CC) == FAILURE) { \
		/* Fallback on original handler */ \
		return php_operator_original_opcode_handlers[PHP_OPERATOR_DECODE(opname,opline)](ZEND_OPCODE_HANDLER_ARGS_PASSTHRU); \
	} \
\
	if (free_op1.var) zval_dtor(free_op1.var); \
	if (free_op2.var) zval_dtor(free_op2.var); \
	execute_data->opline++; \
	return 0; \
}

#define PHP_OPERATOR_UNARY_OP(opname,methodname)		\
static int php_operator_op_##opname	(ZEND_OPCODE_HANDLER_ARGS) \
{ \
	PHP_OPERATOR_GET_OPLINE \
	zend_free_op free_op1; \
	zval *op1 = php_operator_zval_ptr(&(opline->op1), &free_op1, execute_data TSRMLS_CC); \
\
	if (opline->op1.op_type == IS_CONST || \
		op1->type != IS_OBJECT || \
		!zend_hash_exists(&Z_OBJCE_P(op1)->function_table, methodname, sizeof(methodname))) { \
		/* Rely on primary handler */ \
		return php_operator_original_opcode_handlers[PHP_OPERATOR_DECODE(opname,opline)](ZEND_OPCODE_HANDLER_ARGS_PASSTHRU); \
	} \
\
	if (php_operator_method(PHP_OPERATOR_RESULT, op1, methodname, sizeof(methodname) - 1, NULL TSRMLS_CC) == FAILURE) { \
		/* Fallback on original handler */ \
		return php_operator_original_opcode_handlers[PHP_OPERATOR_DECODE(opname,opline)](ZEND_OPCODE_HANDLER_ARGS_PASSTHRU); \
	} \
\
	if (free_op1.var) zval_dtor(free_op1.var); \
	execute_data->opline++; \
	return 0; \
}

PHP_OPERATOR_BINARY_OP(ZEND_ADD,		"__add")
PHP_OPERATOR_BINARY_OP(ZEND_SUB,		"__sub")
PHP_OPERATOR_BINARY_OP(ZEND_MUL,		"__mul")
PHP_OPERATOR_BINARY_OP(ZEND_DIV,		"__div")
PHP_OPERATOR_BINARY_OP(ZEND_MOD,		"__mod")
PHP_OPERATOR_BINARY_OP(ZEND_SL,			"__sl")
PHP_OPERATOR_BINARY_OP(ZEND_SR,			"__sr")
PHP_OPERATOR_BINARY_OP(ZEND_CONCAT,		"__concat")
PHP_OPERATOR_BINARY_OP(ZEND_BW_OR,		"__bw_or")
PHP_OPERATOR_BINARY_OP(ZEND_BW_AND,		"__bw_and")
PHP_OPERATOR_BINARY_OP(ZEND_BW_XOR,		"__bw_xor")

PHP_OPERATOR_UNARY_OP(ZEND_BW_NOT,		"__bw_not")
PHP_OPERATOR_UNARY_OP(ZEND_BOOL,		"__bool")
PHP_OPERATOR_UNARY_OP(ZEND_BOOL_NOT,	"__bool_not")

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

	/* Unaries */
	PHP_OPERATOR_REPLACE_OPCODE(ZEND_BW_NOT);
	PHP_OPERATOR_REPLACE_OPCODE(ZEND_BOOL);
	PHP_OPERATOR_REPLACE_OPCODE(ZEND_BOOL_NOT);

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
	php_info_print_table_header(2, "operator overloading support", "+ - * / % << >> . | & ^ ~ !");
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
	PHP_OPERATOR_EXTVER,
#endif
	STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_OPERATOR
ZEND_GET_MODULE(operator)
#endif

