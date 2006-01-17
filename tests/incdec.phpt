--TEST--
Inc/Dec ops
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

$c = new foo(7);
for($i = 0; $i < 3; $i++) {
  var_dump($c++);
}
for($i = 0; $i < 3; $i++) {
  var_dump($c--);
}
for($i = 0; $i < 3; $i++) {
  var_dump(++$c);
}
for($i = 0; $i < 3; $i++) {
  var_dump(--$c);
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
