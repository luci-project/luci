With Gnat.IO;

package body fib_0 is
   use Gnat.IO;

   Version : constant Integer := 0;

   function FibAlgo(Value : Long_Integer) return Long_Integer is
   begin
      if Value < 2 then
         return Value;
      else
         return FibAlgo(value - 1) + FibAlgo(value - 2);
      end if;
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
    Put_Line("[Ada Fibonacci Library v" & LeftTrim(Version'Image) & "] fib(" & LeftTrim(Value'Image) & ") = " & LeftTrim(fibalgo(value)'Image));
   end PrintFib;

end fib_0;