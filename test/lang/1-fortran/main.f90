program main
  use iso_fortran_env
  use, intrinsic :: iso_c_binding, only: c_long
  implicit none
  
  interface
    function fib(value) result(res) bind(c, name="fib")
      import
      integer(c_long), intent(in) :: value
      integer(c_long) :: res
    end function

    subroutine printfib(value) bind(c, name="printfib")
      import
      integer(c_long), value :: value
    end subroutine
  end interface
  
  integer(c_long) :: i
  write(*, '(A)') "[Fortran main]"
  do i = 0, 2
    if (i /= 0) call sleep(10)
    write(*, 9) "fib(", i, ") = ", fib(i)
  9 format(A, I0, A, I0)
    call flush(output_unit)
    call printfib(21 + i)
  end do

end program main