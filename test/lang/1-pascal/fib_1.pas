library fibs;

Uses sysutils;


const
	version = 1;


function fibalgo (value: Int64) : Int64;
begin
  if value < 0
  then fibalgo := 0
  else if value < 2
  then fibalgo := value
  else fibalgo := fibalgo(value - 1) + fibalgo(value - 2)
end;


function fib (value: Int64) : Int64;
  cdecl; export;
begin
  fib := fibalgo(value)
end;

exports
  fib;


procedure printfib (value: Int64);
   cdecl; export;
var
  r: Int64;
  fmt,s : string;
begin
  r:=fibalgo(value);
  fmt:='[Pascal Fibonacci Library v%d] fib(%d) = %d';
  s:=Format(fmt,[version,value,r]);
  Writeln(s);
  Flush(Output);
end;

exports
  printfib;

end.