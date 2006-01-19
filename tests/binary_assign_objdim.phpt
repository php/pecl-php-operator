--TEST--
binary assign obj/dim ops
--SKIPIF--
<?php if(!extension_loaded("operator")) print "skip"; ?>
--FILE--
<?php
class foo {
	private $value;

	function __assign_add($val) {
		return $this->value += $val;
	}

	function __construct($init) {
		$this->value = $init;
	}
}

class bar implements arrayaccess {
	public $baz;

	function offsetget($ofs) {
		return $this->{$ofs};
	}

	function offsetset($ofs,$val) { echo "argh"; }
	function offsetunset($ofs) { echo "oof"; }
	function offsetexists($ofs) { echo "ick"; return true; }
}

$a = new foo(4);
$b = new bar;
$b->baz = $a;

var_dump($b->baz += 10);
var_dump($b->baz += 5);
var_dump($b['baz'] += 10);
var_dump($b['baz'] += 5);
--EXPECT--
int(14)
int(19)
int(29)
int(34)
