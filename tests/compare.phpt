--TEST--
Basic comparison ops
--SKIPIF--
<?php if(!extension_loaded("operator")) print "skip"; ?>
--FILE--
<?php
class foo {
	private $value;

	function __is_identical($val) {
		return $this->value === $val;
	}

	function __is_not_identical($val) {
		return $this->value !== $val;
	}

	function __is_equal($val) {
		return $this->value == $val;
	}

	function __is_not_equal($val) {
		return $this->value != $val;
	}

	function __is_smaller($val) {
		return $this->value < $val;
	}

	function __is_smaller_or_equal($val) {
		return $this->value <= $val;
	}

	function __construct($init) {
		$this->value = $init;
	}
}

$c = new foo(5);

var_dump($c === 5);
var_dump($c === '5');
var_dump($c !== 5);
var_dump($c !== '5');
var_dump($c == 5);
var_dump($c == '5');
var_dump($c == 6);
var_dump($c != 5);
var_dump($c != '5');
var_dump($c != 6);
var_dump($c < 5);
var_dump($c < 6);
var_dump($c <= 5);
var_dump($c <= 4);
--EXPECT--
bool(true)
bool(false)
bool(false)
bool(true)
bool(true)
bool(true)
bool(false)
bool(false)
bool(false)
bool(true)
bool(false)
bool(true)
bool(true)
bool(false)
