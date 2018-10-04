--TEST--
Bug#56904 Mixing direct calls and implicit overloads
--FILE--
<?php
class a {

public $v = 'initial';

public function __assign_sl($val) {
$this->v = $val;
}
}

$bob = new a;
var_dump($bob);
$bob->__assign_sl('abc');
var_dump($bob);
$bob <<= 'def';
var_dump($bob);
?>
--EXPECTF--
object(a)#%d (1) {
  ["v"]=>
  string(7) "initial"
}
object(a)#%d (1) {
  ["v"]=>
  string(3) "abc"
}
object(a)#%d (1) {
  ["v"]=>
  string(3) "def"
}
