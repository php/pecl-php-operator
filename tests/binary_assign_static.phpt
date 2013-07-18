--TEST--
Standard assign to static property
--SKIPIF--
<?php if(!extension_loaded("operator")) print "skip"; ?>
--FILE--
<?php
class foo {
	static $value;
}

foo::$value = 1;
var_dump(foo::$value);
--EXPECT--
int(1)
