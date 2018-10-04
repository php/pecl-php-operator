dnl config.m4 for extension operators

PHP_ARG_ENABLE(operator, whether to enable operator overload support,
[  --enable-operator           Enable operator overload support])

if test "$PHP_OPERATOR" != "no"; then
  PHP_NEW_EXTENSION(operator, operator.c, $ext_shared)
fi
