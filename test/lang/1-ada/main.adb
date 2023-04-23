with Ada.Text_IO;
with Ada.Strings.Fixed;
with Interfaces.C;

procedure Main is
   use Ada.Text_IO;
   use Ada.Strings.Fixed;

   function Fib(Value : Long_Integer) return Long_Integer;
   procedure PrintFib(Value : Long_Integer);

   pragma Import (C, PrintFib, "printfib");
   pragma Import (C, Fib, "fib");
   
   pragma Linker_Options ("-lfib");

begin
   Put_Line("[Ada main]");
   for I in 0 .. 2 loop
      if I /= 0 then
         delay 10.0; -- sleep for 10 seconds
      end if;
      Put_Line("fib(" & Trim(Long_Integer'Image(Long_Integer(I)), Ada.Strings.Left) & ") = " &  Trim(Long_Integer'Image(Fib(Long_Integer(I))), Ada.Strings.Left));
      PrintFib(Long_Integer(21 + I));
      Flush;
   end loop;
end Main;