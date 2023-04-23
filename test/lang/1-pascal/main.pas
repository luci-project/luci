program Main;

uses SysUtils;

{$linklib fib}
{$PIC ON}

function fib(value: Int64): Int64; cdecl; external;
procedure printfib(value: Int64); cdecl; external;

var
  i: Int64;
begin
  writeln('[Pascal main]');
  for i := 0 to 2 do
  begin
    if i <> 0 then
      sleep(10000);
    writeln('fib(', i, ') = ', fib(i));
    flush(output);
    printfib(21 + i);
  end;
end.