--TEST--
Basic unary ops
--SKIPIF--
<?php if(!extension_loaded("operator")) print "skip"; ?>
--FILE--
<?php
class foo {
	private $value;

	function __bw_not() {
		return ~$this->value;
	}

	function __bool() {
		return (bool)$this->value;
	}

	function __bool_not() {
		return !(bool)$this->value;
	}

	function __construct($init) {
		$this->value = $init;
	}
}

$c = new foo(7);
var_dump(~$c);
var_dump((bool)$c);
var_dump(!$c);
--EXPECT--
int(-8)
bool(true)
bool(false)
