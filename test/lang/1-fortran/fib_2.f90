module fibonacci_module
   use iso_fortran_env
   use, intrinsic :: iso_c_binding, only: c_long
   implicit none
   integer, parameter :: version = 2

   private
   public :: version, fib, printfib

contains

   recursive function fibalgo(value) result(n)
      integer(c_long), intent(in) :: value
      integer(c_long) :: i, l, n, p

      l = 0
      p = 1
      n = value
      
      do i = 1, value-1
         n = l + p
         l = p
         p = n
      end do
   end function fibalgo

   function fib(value) result(res) bind(c, name="fib")
      integer(c_long), intent(in) :: value
      integer(c_long) :: res
      res = fibalgo(value)
   end function fib

   subroutine printfib(value) bind(c, name="printfib")
      integer(c_long),value :: value
      write(*,99) "[Fortran Fibonacci Library v", version, "] fib(", value, ") = ", fibalgo(value)
   99 format(A, I0, A, I0, A, I0) 
      call flush(output_unit)
   end subroutine printfib

end module fibonacci_module
