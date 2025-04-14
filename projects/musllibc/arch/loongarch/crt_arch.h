__asm__("\
.global _start\n\
.align  4\n\
_start:\n\
  add.d $a0, $zero, $sp\n\
  la   $fp, _start_c\n\
  jirl $ra, $fp, 0\n\
");
