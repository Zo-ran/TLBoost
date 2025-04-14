.global __set_thread_area
.type   __set_thread_area,@function
__set_thread_area:
	or	$t1, $a0, $r0
	ret
