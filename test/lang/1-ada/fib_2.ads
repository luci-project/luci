package fib_2 is
   procedure PrintFib(Value : Long_Integer);
   function Fib(Value : Long_Integer) return Long_Integer;
   
   pragma Export (C, PrintFib, "printfib");
   pragma Export (C, Fib, "fib");
end fib_2;