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

#ifndef PHP_OPERATOR_H
#define PHP_OPERATOR_H

#define PHP_OPERATOR_EXTNAME	"operator"
#define PHP_OPERATOR_VERSION	"0.4.0-dev"

#if ZEND_MODULE_API_NO > 20121211
#define ZEND_ENGINE_2_5
#endif
#if ZEND_MODULE_API_NO > 20100524
#define ZEND_ENGINE_2_4
#endif
#if ZEND_MODULE_API_NO > 20090625
#define ZEND_ENGINE_2_3
#endif
#if ZEND_MODULE_API_NO > 20050922
#define ZEND_ENGINE_2_2
#endif
#if ZEND_MODULE_API_NO > 20050921
#define ZEND_ENGINE_2_1
#endif

#ifdef ZEND_ENGINE_2_4
# define PHP_OPERATOR_OP_TYPE(zop) (zop##_type)
# define PHP_OPERATOR_OP_U(zop) (zop)
# define PHP_OPERATOR_LITERAL_DC , const struct _zend_literal *key
# define PHP_OPERATOR_LITERAL_CC , key
# define PHP_OPERATOR_LITERAL_NULL_CC , NULL
#else
# define PHP_OPERATOR_OP_TYPE(zop) ((zop).op_type)
# define PHP_OPERATOR_OP_U(zop) ((zop).u)
# define PHP_OPERATOR_LITERAL_DC
# define PHP_OPERATOR_LITERAL_CC
# define PHP_OPERATOR_LITERAL_NULL_CC
#endif

#if defined(ZEND_ENGINE_2) && !defined(ZEND_ENGINE_2_1)
typedef struct {
	zval *var;
} zend_free_op;
#endif

#ifndef EX_CV_NUM
# define EX_CV_NUM(ex, n) (&((ex)->CVs[(n)]))
#endif

extern zend_module_entry operator_module_entry;
#define phpext_operator_ptr &operator_module_entry

#endif


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
