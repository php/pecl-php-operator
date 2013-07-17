--TEST--
Basic binary assign ops
--SKIPIF--
<?php if(!extension_loaded("operator")) print "skip"; ?>
--FILE--
<?php
class foo {
	private $value;

	function __assign($val) {
		return $this->value = $val;
	}

	function __assign_add($val) {
		return $this->value += $val;
	}

	function __assign_sub($val) {
		return $this->value -= $val;
	}

	function __assign_mul($val) {
		return $this->value *= $val;
	}

	function __assign_div($val) {
		return $this->value /= $val;
	}

	function __assign_mod($val) {
		return $this->value %= $val;
	}

	function __assign_sl($val) {
		return $this->value <<= $val;
	}

	function __assign_sr($val) {
		return $this->value >>= $val;
	}

	function __assign_concat($val) {
		return $this->value .= $val;
	}

	function __assign_bw_or($val) {
		return $this->value |= $val;
	}

	function __assign_bw_and($val) {
		return $this->value &= $val;
	}

	function __assign_bw_xor($val) {
		return $this->value ^= $val;
	}

	function __construct($init) {
		$this->value = $init;
	}
}

$a = new foo(1);
var_dump($a  = 2);
var_dump($a += 2);
var_dump(is_object($a));

$c = new foo(4);

var_dump($c += 3);
var_dump($c -= 3);
var_dump($c *= 3);
var_dump($c /= 2);
var_dump($c %= 3);

$d = new foo(14);
var_dump($d <<= 2);
var_dump($d >>= 2);

$e = new foo('PHP');
var_dump($e .= ' with');
var_dump($e .= ' overloading');

$f = new foo(0x5A);
var_dump($f |= 0xAA);
var_dump($f &= 0xAA);
var_dump($f ^= 0xAA);
--EXPECT--
int(2)
int(4)
bool(true)
int(7)
int(4)
int(12)
int(6)
int(0)
int(56)
int(14)
string(8) "PHP with"
string(20) "PHP with overloading"
int(250)
int(170)
int(0)
