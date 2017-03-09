--TEST--
Extended comparison ops
--SKIPIF--
<?php if(!extension_loaded("operator")) print "skip";
--FILE--
<?php
class foo {
	private $value;
	function __is_greater($val) {
		return $this->value > $val;
	}
	function __is_greater_or_equal($val) {
		return $this->value >= $val;
	}
	function __construct($init) {
		$this->value = $init;
	}
}
$c = new foo(5);
var_dump($c > 5);
var_dump($c > 4);
var_dump($c >= 5);
var_dump($c >= 6);
--EXPECT--
bool(false)
bool(true)
bool(true)
bool(false)
