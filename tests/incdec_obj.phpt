--TEST--
Inc/Dec obj ops
--SKIPIF--
<?php if(!extension_loaded("operator")) print "skip"; ?>
--FILE--
<?php
class foo {
	private $value;

	function __post_inc() {
		return $this->value++;
	}

	function __post_dec() {
		return $this->value--;
	}

	function __pre_inc() {
		$this->value++;
		return $this->value;
	}

	function __pre_dec() {
		$this->value--;
		return $this->value;
	}

	function __construct($init) {
		$this->value = $init;
	}
}

class bar {
	public $baz;
}

$c = new foo(7);
$d = new bar;
$d->baz = $c;

for($i = 0; $i < 3; $i++) {
  var_dump($d->baz++);
}
for($i = 0; $i < 3; $i++) {
  var_dump($d->baz--);
}
for($i = 0; $i < 3; $i++) {
  var_dump(++$d->baz);
}
for($i = 0; $i < 3; $i++) {
  var_dump(--$d->baz);
}
--EXPECT--
int(7)
int(8)
int(9)
int(10)
int(9)
int(8)
int(8)
int(9)
int(10)
int(9)
int(8)
int(7)
