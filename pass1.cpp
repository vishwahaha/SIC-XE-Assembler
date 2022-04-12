#include "utils.cpp"
#include "tables.cpp"

using namespace std;

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
//------------------------PASS 1 BELOW------------------------------------------
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

string file_name;
bool error_flag = false;
int program_length;
string *block_num_to_name;

string first_executable_sec;

void handle_LTORG(string &lit_prefix, int &line_number_delta, int line_number, int &LOCCTR, int &last_delta_LOCCTR, int current_block_number)
{
  string lit_address, lit_value;
  lit_prefix = "";
  for (auto const &it : LITTAB)
  {
    lit_address = it.second.address;
    lit_value = it.second.value;
    if (lit_address != "?")
    {
      /*address is assigned*/
    }
    else
    {
      line_number += 5;
      line_number_delta += 5;
      LITTAB[it.first].address = int_to_string_hex(LOCCTR);
      LITTAB[it.first].block_number = current_block_number;
      lit_prefix += "\n" + to_string(line_number) + "\t" + int_to_string_hex(LOCCTR) + "\t" + to_string(current_block_number) + "\t" + "*" + "\t" + "=" + lit_value + "\t" + " " + "\t" + " ";

      if (lit_value[0] == 'X')
      {
        LOCCTR += (lit_value.length() - 3) / 2;
        last_delta_LOCCTR += (lit_value.length() - 3) / 2;
      }
      else if (lit_value[0] == 'C')
      {
        LOCCTR += lit_value.length() - 3;
        last_delta_LOCCTR += lit_value.length() - 3;
      }
    }
  }
}

void evaluate_expression(string expression, bool &relative, string &temp_operand, int line_number, ofstream &error_file, bool &error_flag)
{
  string single_operand = "?", single_operator = "?", value_string = "", value_temp = "", write_data = "";
  int last_operand = 0, last_operator = 0, pair_count = 0;
  char last_byte = ' ';
  bool illegal = false;

  for (int i = 0; i < expression.length();)
  {
    single_operand = "";

    last_byte = expression[i];
    while ((last_byte != '+' && last_byte != '-' && last_byte != '/' && last_byte != '*') && i < expression.length())
    {
      single_operand += last_byte;
      last_byte = expression[++i];
    }

    if (SYMTAB[single_operand].exists == 'y')
    {
      last_operand = SYMTAB[single_operand].relative;
      value_temp = to_string(string_hex_to_int(SYMTAB[single_operand].address));
    }
    else if ((single_operand != "" || single_operand != "?") && if_all_num(single_operand))
    {
      last_operand = 0;
      value_temp = single_operand;
    }
    else
    {
      write_data = "Line: " + to_string(line_number) + " : Can't find symbol. Found " + single_operand;
      write_to_file(error_file, write_data);
      illegal = true;
      break;
    }

    if (last_operand * last_operator == 1)
    {
      write_data = "Line: " + to_string(line_number) + " : illegal expression";
      write_to_file(error_file, write_data);
      error_flag = true;
      illegal = true;
      break;
    }
    else if ((single_operator == "-" || single_operator == "+" || single_operator == "?") && last_operand == 1)
    {
      if (single_operator == "-")
      {
        pair_count--;
      }
      else
      {
        pair_count++;
      }
    }

    value_string += value_temp;

    single_operator = "";
    while (i < expression.length() && (last_byte == '+' || last_byte == '-' || last_byte == '/' || last_byte == '*'))
    {
      single_operator += last_byte;
      last_byte = expression[++i];
    }

    if (single_operator.length() > 1)
    {
      write_data = "Line: " + to_string(line_number) + " : illegal operator in expression. Found " + single_operator;
      write_to_file(error_file, write_data);
      error_flag = true;
      illegal = true;
      break;
    }

    if (single_operator == "*" || single_operator == "/")
    {
      last_operator = 1;
    }
    else
    {
      last_operator = 0;
    }

    value_string += single_operator;
  }

  if (!illegal)
  {
    if (pair_count == 1)
    {
      /*relative*/
      relative = 1;
      EvaluateString stash_instance(value_string);
      temp_operand = int_to_string_hex(stash_instance.get_result());
    }
    else if (pair_count == 0)
    {
      /*absolute*/
      relative = 0;
      cout << value_string << endl;
      EvaluateString stash_instance(value_string);
      temp_operand = int_to_string_hex(stash_instance.get_result());
    }
    else
    {
      write_data = "Line: " + to_string(line_number) + " : illegal expression";
      write_to_file(error_file, write_data);
      error_flag = true;
      temp_operand = "00000";
      relative = 0;
    }
  }
  else
  {
    temp_operand = "00000";
    error_flag = true;
    relative = 0;
  }
}
void pass1()
{
  ifstream source_file; // begin
  ofstream intermediate_file, error_file;

  source_file.open(file_name);
  if (!source_file)
  {
    cout << "Unable to open file: " << file_name << endl;
    exit(1);
  }

  intermediate_file.open("intermediate_" + file_name);
  if (!intermediate_file)
  {
    cout << "Unable to open file: intermediate_" << file_name << endl;
    exit(1);
  }
  write_to_file(intermediate_file, "Line\tAddress\tLabel\tOPCODE\tOPERAND\tComment");
  error_file.open("error_" + file_name);
  if (!error_file)
  {
    cout << "Unable to open file: error_" << file_name << endl;
    exit(1);
  }
  write_to_file(error_file, "-----------------PASS1---------------");

  string file_line;
  string write_data, write_data_suffix = "", write_data_prefix = "";
  int index = 0;

  string current_block_name = "DEFAULT";
  int current_block_number = 0;
  int total_blocks = 1;

  bool status_code;
  string label, opcode, operand, comment;
  string temp_operand;

  int start_address, LOCCTR, save_LOCCTR, line_number, last_delta_LOCCTR, line_number_delta = 0;
  line_number = 0;
  last_delta_LOCCTR = 0;

  getline(source_file, file_line);
  line_number += 5;
  while (check_comment_line(file_line))
  {
    write_data = to_string(line_number) + "\t" + file_line;
    write_to_file(intermediate_file, write_data);
    getline(source_file, file_line);
    line_number += 5;
    index = 0;
  }

  read_first_non_white_space(file_line, index, status_code, label);
  read_first_non_white_space(file_line, index, status_code, opcode);

  if (opcode == "START")
  {
    read_first_non_white_space(file_line, index, status_code, operand);
    read_first_non_white_space(file_line, index, status_code, comment, true);
    start_address = string_hex_to_int(operand);
    LOCCTR = start_address;
    write_data = to_string(line_number) + "\t" + int_to_string_hex(LOCCTR - last_delta_LOCCTR) + "\t" + to_string(current_block_number) + "\t" + label + "\t" + opcode + "\t" + operand + "\t" + comment;
    write_to_file(intermediate_file, write_data);

    getline(source_file, file_line);
    line_number += 5;
    index = 0;
    read_first_non_white_space(file_line, index, status_code, label);
    read_first_non_white_space(file_line, index, status_code, opcode);
  }
  else
  {
    start_address = 0;
    LOCCTR = 0;
  }
  string current_sect_name = "DEFAULT";
  int section_counter = 0;
  while (opcode != "END")
  {
    while (opcode != "END" && opcode != "CSECT")
    {
      if (!check_comment_line(file_line))
      {
        if (label != "")
        {
          if (SYMTAB[label].exists == 'n')
          {
            SYMTAB[label].name = label;
            SYMTAB[label].address = int_to_string_hex(LOCCTR);
            SYMTAB[label].relative = 1;
            SYMTAB[label].exists = 'y';
            SYMTAB[label].block_number = current_block_number;
          }
          else
          {
            write_data = "Line: " + to_string(line_number) + " : Duplicate symbol for '" + label + "'. Previously defined at " + SYMTAB[label].address;
            write_to_file(error_file, write_data);
            error_flag = true;
          }
        }
        if (OPTAB[get_real_opcode(opcode)].exists == 'y')
        {
          if (OPTAB[get_real_opcode(opcode)].format == 3)
          {
            LOCCTR += 3;
            last_delta_LOCCTR += 3;
            if (get_flag_format(opcode) == '+')
            {
              LOCCTR += 1;
              last_delta_LOCCTR += 1;
            }
            if (get_real_opcode(opcode) == "RSUB")
            {
              operand = " ";
            }
            else
            {
              read_first_non_white_space(file_line, index, status_code, operand);
              if (operand[operand.length() - 1] == ',')
              {
                read_first_non_white_space(file_line, index, status_code, temp_operand);
                operand += temp_operand;
              }
            }

            if (get_flag_format(operand) == '=')
            {
              temp_operand = operand.substr(1, operand.length() - 1);
              if (temp_operand == "*")
              {
                temp_operand = "X'" + int_to_string_hex(LOCCTR - last_delta_LOCCTR, 6) + "'";
              }
              if (LITTAB[temp_operand].exists == 'n')
              {
                LITTAB[temp_operand].value = temp_operand;
                LITTAB[temp_operand].exists = 'y';
                LITTAB[temp_operand].address = "?";
                LITTAB[temp_operand].block_number = -1;
              }
            }
          }
          else if (OPTAB[get_real_opcode(opcode)].format == 1)
          {
            operand = " ";
            LOCCTR += OPTAB[get_real_opcode(opcode)].format;
            last_delta_LOCCTR += OPTAB[get_real_opcode(opcode)].format;
          }
          else
          {
            LOCCTR += OPTAB[get_real_opcode(opcode)].format;
            last_delta_LOCCTR += OPTAB[get_real_opcode(opcode)].format;
            read_first_non_white_space(file_line, index, status_code, operand);
            if (operand[operand.length() - 1] == ',')
            {
              read_first_non_white_space(file_line, index, status_code, temp_operand);
              operand += temp_operand;
            }
          }
        }
        else if (opcode == "WORD")
        {
          read_first_non_white_space(file_line, index, status_code, operand);
          LOCCTR += 3;
          last_delta_LOCCTR += 3;
        }
        else if (opcode == "RESW")
        {
          read_first_non_white_space(file_line, index, status_code, operand);
          LOCCTR += 3 * stoi(operand);
          last_delta_LOCCTR += 3 * stoi(operand);
        }
        else if (opcode == "RESB")
        {
          read_first_non_white_space(file_line, index, status_code, operand);
          LOCCTR += stoi(operand);
          last_delta_LOCCTR += stoi(operand);
        }
        else if (opcode == "BYTE")
        {
          read_byte_operand(file_line, index, status_code, operand);
          if (operand[0] == 'X')
          {
            LOCCTR += (operand.length() - 3) / 2;
            last_delta_LOCCTR += (operand.length() - 3) / 2;
          }
          else if (operand[0] == 'C')
          {
            LOCCTR += operand.length() - 3;
            last_delta_LOCCTR += operand.length() - 3;
          }
        }
        else if (opcode == "BASE")
        {
          read_first_non_white_space(file_line, index, status_code, operand);
        }
        else if (opcode == "LTORG")
        {
          operand = " ";
          handle_LTORG(write_data_suffix, line_number_delta, line_number, LOCCTR, last_delta_LOCCTR, current_block_number);
        }
        else if (opcode == "ORG")
        {
          read_first_non_white_space(file_line, index, status_code, operand);

          char last_byte = operand[operand.length() - 1];
          while (last_byte == '+' || last_byte == '-' || last_byte == '/' || last_byte == '*')
          {
            read_first_non_white_space(file_line, index, status_code, temp_operand);
            operand += temp_operand;
            last_byte = operand[operand.length() - 1];
          }

          int swap_var;
          swap_var = save_LOCCTR;
          save_LOCCTR = LOCCTR;
          LOCCTR = swap_var;

          if (SYMTAB[operand].exists == 'y')
          {
            LOCCTR = string_hex_to_int(SYMTAB[operand].address);
          }
          else
          {
            bool relative;
            error_flag = false;
            evaluate_expression(operand, relative, temp_operand, line_number, error_file, error_flag);
            if (!error_flag)
            {
              LOCCTR = string_hex_to_int(temp_operand);
            }
            error_flag = false;
          }
        }
        else if (opcode == "USE")
        {

          read_first_non_white_space(file_line, index, status_code, operand);
          BLOCKS[current_block_name].LOCCTR = int_to_string_hex(LOCCTR);

          if (BLOCKS[operand].exists == 'n')
          {
            BLOCKS[operand].exists = 'y';
            BLOCKS[operand].name = operand;
            BLOCKS[operand].number = total_blocks++;
            BLOCKS[operand].LOCCTR = "0";
          }

          current_block_number = BLOCKS[operand].number;
          current_block_name = BLOCKS[operand].name;
          LOCCTR = string_hex_to_int(BLOCKS[operand].LOCCTR);
        }
        else if (opcode == "EQU")
        {
          read_first_non_white_space(file_line, index, status_code, operand);
          temp_operand = "";
          bool relative;

          if (operand == "*")
          {
            temp_operand = int_to_string_hex(LOCCTR - last_delta_LOCCTR, 6);
            relative = 1;
          }
          else if (if_all_num(operand))
          {
            temp_operand = int_to_string_hex(stoi(operand), 6);
            relative = 0;
          }
          else
          {
            char last_byte = operand[operand.length() - 1];

            while (last_byte == '+' || last_byte == '-' || last_byte == '/' || last_byte == '*')
            {
              read_first_non_white_space(file_line, index, status_code, temp_operand);
              operand += temp_operand;
              last_byte = operand[operand.length() - 1];
            }
            evaluate_expression(operand, relative, temp_operand, line_number, error_file, error_flag);
          }

          SYMTAB[label].name = label;
          SYMTAB[label].address = temp_operand;
          SYMTAB[label].relative = relative;
          SYMTAB[label].block_number = current_block_number;
          last_delta_LOCCTR = LOCCTR - string_hex_to_int(temp_operand);
        }
        else
        {
          read_first_non_white_space(file_line, index, status_code, operand);
          write_data = "Line: " + to_string(line_number) + " : Invalid OPCODE. Found " + opcode;
          write_to_file(error_file, write_data);
          error_flag = true;
        }
        read_first_non_white_space(file_line, index, status_code, comment, true);
        if (opcode == "EQU" && SYMTAB[label].relative == 0)
        {
          write_data = write_data_prefix + to_string(line_number) + "\t" + int_to_string_hex(LOCCTR - last_delta_LOCCTR) + "\t" + " " + "\t" + label + "\t" + opcode + "\t" + operand + "\t" + comment + write_data_suffix;
        }
        else
        {
          write_data = write_data_prefix + to_string(line_number) + "\t" + int_to_string_hex(LOCCTR - last_delta_LOCCTR) + "\t" + to_string(current_block_number) + "\t" + label + "\t" + opcode + "\t" + operand + "\t" + comment + write_data_suffix;
        }
        write_data_prefix = "";
        write_data_suffix = "";
      }
      else
      {
        write_data = to_string(line_number) + "\t" + file_line;
      }
      write_to_file(intermediate_file, write_data);

      BLOCKS[current_block_name].LOCCTR = int_to_string_hex(LOCCTR);
      getline(source_file, file_line);                              
      line_number += 5 + line_number_delta;
      line_number_delta = 0;
      index = 0;
      last_delta_LOCCTR = 0;
      read_first_non_white_space(file_line, index, status_code, label);
      read_first_non_white_space(file_line, index, status_code, opcode); 
    }

    if (opcode != "END")
    {

      if (SYMTAB[label].exists == 'n')
      {
        SYMTAB[label].name = label;
        SYMTAB[label].address = int_to_string_hex(LOCCTR);
        SYMTAB[label].relative = 1;
        SYMTAB[label].exists = 'y';
        SYMTAB[label].block_number = current_block_number;
      }

      write_to_file(intermediate_file, write_data_prefix + to_string(line_number) + "\t" + int_to_string_hex(LOCCTR - last_delta_LOCCTR) + "\t" + to_string(current_block_number) + "\t" + label + "\t" + opcode);

      getline(source_file, file_line);
      line_number += 5;

      read_first_non_white_space(file_line, index, status_code, label);
      read_first_non_white_space(file_line, index, status_code, opcode);
    }
  }

  if (opcode == "END")
  {
    first_executable_sec = SYMTAB[label].address;
    SYMTAB[first_executable_sec].name = label;
    SYMTAB[first_executable_sec].address = first_executable_sec;
  }

  read_first_non_white_space(file_line, index, status_code, operand);
  read_first_non_white_space(file_line, index, status_code, comment, true);

  current_block_name = "DEFAULT";
  current_block_number = 0;
  LOCCTR = string_hex_to_int(BLOCKS[current_block_name].LOCCTR);

  handle_LTORG(write_data_suffix, line_number_delta, line_number, LOCCTR, last_delta_LOCCTR, current_block_number);

  write_data = to_string(line_number) + "\t" + int_to_string_hex(LOCCTR - last_delta_LOCCTR) + "\t" + " " + "\t" + label + "\t" + opcode + "\t" + operand + "\t" + comment + write_data_suffix;
  write_to_file(intermediate_file, write_data);

  int LOCCTR_arr[total_blocks];
  block_num_to_name = new string[total_blocks];
  for (auto const &it : BLOCKS)
  {
    LOCCTR_arr[it.second.number] = string_hex_to_int(it.second.LOCCTR);
    block_num_to_name[it.second.number] = it.first;
  }

  for (int i = 1; i < total_blocks; i++)
  {
    LOCCTR_arr[i] += LOCCTR_arr[i - 1];
  }

  for (auto const &it : BLOCKS)
  {
    if (it.second.start_address == "?")
    {
      BLOCKS[it.first].start_address = int_to_string_hex(LOCCTR_arr[it.second.number - 1]);
    }
  }

  program_length = LOCCTR_arr[total_blocks - 1] - start_address;

  source_file.close();
  intermediate_file.close();
  error_file.close();
}
