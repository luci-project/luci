library fibs;

Uses sysutils;


const
	version = 2;


function fibalgo (value: Int64) : Int64;
var
  l,p,n,i: Int64;
begin
  l := 0;
  p := 1;
  n := value;
  for i := 2 to value do
  begin
    n := l + p;
    l := p;
    p := n;
  end;
  fibalgo := n
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