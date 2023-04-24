With Gnat.IO;

package body fib_2 is
   use Gnat.IO;
   
   Version : constant Integer := 2;

   function FibAlgo(Value : Long_Integer) return Long_Integer is
      L : Long_Integer := 0;
      P : Long_Integer := 1;
      N : Long_Integer := Value;
   begin
      if Value < 2 then
         return Value;
      end if;
      
      for I in 1..Value-1 loop
         N := L + P;
         L := P;
         P := N;
      end loop;
      
      return N;
   end FibAlgo;

   function Fib(Value : Long_Integer) return Long_Integer is
   begin
      return FibAlgo(Value);
   end Fib;

   function LeftTrim(Str : string) return string is
   begin
     if Str'length > 1 and Str(Str'first) = ' ' then
       return LeftTrim(Str(Str'first + 1..Str'last));
     else
       return Str;
     end if;
   end LeftTrim;

   procedure PrintFib(Value : Long_Integer) is
   begin
    Put_Line("[Ada Fibonacci Library v" & LeftTrim(Integer'Image(Version)) & "] fib(" & LeftTrim(Long_Integer'Image(Value)) & ") = " & LeftTrim(Long_Integer'Image(fibalgo(value))));
   end PrintFib;

end fib_2;