#include <stdio.h>
#include <math.h>

int fib(int n) {
  double phi = (1 + sqrt(5)) / 2;
  return round(pow(phi, n) / sqrt(5));
}
 
// Driver Code
int main ()
{
  int n = 9;
  printf("%d\n", fib(n));
  return 0;
}

