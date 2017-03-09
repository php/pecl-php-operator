# Operator overloading extension for PHP7

## Usage

Implement magic methods in a class to provide operator overloading automatically, for example:

```php
class C {
  private $value = 0;
  public function __add($delta) {
    $this->value += $delta;
    return $this;
  }

  public function __toString() {
    return strval($this->value);
  }
}

$c = new C;
var_dump($c + 5); // object(C)#1 { ["C:value:private"]=>5 }
```

The following overload methods are supported:

| Operator | Method |
|:---:| --- |
| $o + $arg | `__add($arg)` |
| $o - $arg | `__sub($arg)` |
| $o * $arg | `__mul($arg)` |
| $o / $arg | `__div($arg)` |
| $o % $arg | `__mod($arg)` |
| $o ** $arg | `__pow($arg)` |
| $o << $arg | `__sl($arg)` |
| $o >> $arg | `__sr($arg)` |
| $o . $arg | `__concat($arg)` |
| $o &#x7c; $arg | `__bw_or($arg)` |
| $o & $arg | `__bw_and($arg)` |
| $o ^ $arg | `__bw_xor($arg)` |
| ~$o | `__bw_not()` |
| $o === $arg | `__is_identical($arg)` |
| $o !== $arg | `__is_not_identical($arg)` |
| $o == $arg | `__is_equal($arg)` |
| $o != $arg | `__is_not_equal($arg)` |
| $o < $arg | `__is_smaller($arg)` |
| $o <= $arg | `__is_smaller_or_equal($arg)` |
| $o > $arg | `__is_greater($arg)` &#42; |
| $o >= $arg | `__is_greater_or_equal($arg)` &#42; |
| $o <=> $arg | `__cmp($arg)` |
| ++$o | `__pre_inc()` |
| $o++  | `__post_inc()` |
| --$o | `__pre_dec()` |
| $o-- | `__post_dec()` |
| $o = $arg | `__assign($arg)` |
| $o += $arg | `__assign_add($arg)` |
| $o -= $arg | `__assign_sub($arg)` |
| $o *= $arg | `__assign_mul($arg)` |
| $o /= $arg | `__assign_div($arg)` |
| $o %= $arg | `__assign_mod($arg)` |
| $o **= $arg | `__assign_pow($arg)` |
| $o <<= $arg | `__assign_sl($arg)` |
| $o >>= $arg | `__assign_sr($arg)` |
| $o .= $arg | `__assign_concat($arg)` |
| $o &#x7c;= $arg | `__assign_bw_or($arg)` |
| $o &= $arg | `__assign_bw_and($arg)` |
| $o ^= $arg | `__assign_bw_xor($arg)` |

&#42; - `__is_greater()` and `__is_greater_or_equal()` require a rebuild of the main PHP runtime using the [included patch](php7-is_greater.diff). Withtout this patch, `$a > $b` is automatically remapped to `$b < $a` by the engine.
