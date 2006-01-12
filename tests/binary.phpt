--TEST--
Basic binary ops
--SKIPIF--
<?php if(!extension_loaded("operator")) print "skip"; ?>
--FILE--
<?php
class foo {
	private $value;

	function __add($val) {
		return $this->value + $val;
	}

	function __sub($val) {
		return $this->value - $val;
	}

	function __mul($val) {
		return $this->value * $val;
	}

	function __div($val) {
		return $this->value / $val;
	}

	function __mod($val) {
		return $this->value % $val;
	}

	function __sl($val) {
		return $this->value << $val;
	}

	function __sr($val) {
		return $this->value >> $val;
	}

	function __concat($val) {
		return $this->value . $val;
	}

	function __bw_or($val) {
		return $this->value | $val;
	}

	function __bw_and($val) {
		return $this->value & $val;
	}

	function __bw_xor($val) {
		return $this->value ^ $val;
	}

	function __construct($init) {
		$this->value = $init;
	}
}

$c = new foo(7);

var_dump($c + 3);
var_dump($c - 3);
var_dump($c * 3);
var_dump($c / 2);
var_dump($c % 3);

$d = new foo(14);
var_dump($d << 2);
var_dump($d >> 2);

$e = new foo('PHP');
var_dump($e . ' with overloading');

$f = new foo(0x5A);
var_dump($f | 0xAA);
var_dump($f & 0xAA);
var_dump($f ^ 0xAA);
--EXPECT--
int(10)
int(4)
int(21)
float(3.5)
int(1)
int(56)
int(3)
string(20) "PHP with overloading"
int(250)
int(10)
int(240)
