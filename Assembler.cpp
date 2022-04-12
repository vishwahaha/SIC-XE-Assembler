#include "pass2.cpp"
using namespace std;

int main()
{
  cout << "Input file executable must be in same directory" << endl
       << endl;
  cout << "Name of input file:";
  cin >> file_name;

  cout << "\nInitializing OPTAB" << endl;
  load_tables();

  cout << "\n-------------Pass1--------------" << endl;
  cout << "Starting to write intermediate file to 'intermediate_" << file_name << "'" << endl;
  cout << "Starting to write error file to 'error_" << file_name << "'" << endl;
  pass1();

  cout << "\n-------------Pass2--------------" << endl;
  cout << "Starting to write object file to 'object_" << file_name << "'" << endl;
  cout << "Starting to write listing file to 'listing_" << file_name << "'" << endl;
  pass2();
}