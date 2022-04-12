#include "pass1.cpp"

using namespace std;

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
//------------------------PASS 2 BELOW------------------------------------------
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

ifstream intermediate_file;
ofstream error_file, object_file, listing_file;
ofstream printtab;
string writestring;

bool is_comment;
string label, opcode, operand, comment;
string operand1, operand2;

int line_number, block_number, address, start_address;
string object_code, write_data, current_record, modification_record = "M^", end_record, write_R_Data, write_D_Data, current_sect_name = "DEFAULT";
int section_counter = 0;
int program_section_length = 0;

int program_counter, base_register_value, current_text_record_length;
bool nobase;

string read_till_tab(string data, int &index)
{
  string temp_buff = "";

  while (index < data.length() && data[index] != '\t')
  {
    temp_buff += data[index];
    index++;
  }
  index++;
  if (temp_buff == " ")
  {
    temp_buff = "-1";
  }
  return temp_buff;
}

bool read_intermediate_file(ifstream &read_file, bool &is_comment, int &line_number, int &address, int &block_number, string &label, string &opcode, string &operand, string &comment)
{
  string file_line = "";
  string temp_buff = "";
  bool tempStatus;
  int index = 0;
  if (!getline(read_file, file_line))
  {
    return false;
  }
  line_number = stoi(read_till_tab(file_line, index));

  is_comment = (file_line[index] == '.') ? true : false;
  if (is_comment)
  {
    read_first_non_white_space(file_line, index, tempStatus, comment, true);
    return true;
  }
  address = string_hex_to_int(read_till_tab(file_line, index));
  temp_buff = read_till_tab(file_line, index);
  if (temp_buff == " ")
  {
    block_number = -1;
  }
  else
  {
    block_number = stoi(temp_buff);
  }
  label = read_till_tab(file_line, index);
  if (label == "-1")
  {
    label = " ";
  }
  opcode = read_till_tab(file_line, index);
  if (opcode == "BYTE")
  {
    read_byte_operand(file_line, index, tempStatus, operand);
  }
  else
  {
    operand = read_till_tab(file_line, index);
    if (operand == "-1")
    {
      operand = " ";
    }
    if (opcode == "CSECT")
    {
      return true;
    }
  }
  read_first_non_white_space(file_line, index, tempStatus, comment, true);
  return true;
}

string create_object_code_format34()
{
  string objcode;
  int half_bytes;
  half_bytes = (get_flag_format(opcode) == '+') ? 5 : 3;

  if (get_flag_format(operand) == '#')
  {
    if (operand.substr(operand.length() - 2, 2) == ",X")
    {
      write_data = "Line: " + to_string(line_number) + " Index based addressing not supported with Indirect addressing";
      write_to_file(error_file, write_data);
      objcode = int_to_string_hex(string_hex_to_int(OPTAB[get_real_opcode(opcode)].opcode) + 1, 2);
      objcode += (half_bytes == 5) ? "100000" : "0000";
      return objcode;
    }

    string temp_operand = operand.substr(1, operand.length() - 1);
    if (if_all_num(temp_operand) || ((SYMTAB[temp_operand].exists == 'y') && (SYMTAB[temp_operand].relative == 0)))
    {
      int immediate_value;

      if (if_all_num(temp_operand))
      {
        immediate_value = stoi(temp_operand);
      }
      else
      {
        immediate_value = string_hex_to_int(SYMTAB[temp_operand].address);
      }
      if (immediate_value >= (1 << 4 * half_bytes))
      {
        write_data = "Line: " + to_string(line_number) + " Immediate value exceeds format limit";
        write_to_file(error_file, write_data);
        objcode = int_to_string_hex(string_hex_to_int(OPTAB[get_real_opcode(opcode)].opcode) + 1, 2);
        objcode += (half_bytes == 5) ? "100000" : "0000";
      }
      else
      {
        objcode = int_to_string_hex(string_hex_to_int(OPTAB[get_real_opcode(opcode)].opcode) + 1, 2);
        objcode += (half_bytes == 5) ? '1' : '0';
        objcode += int_to_string_hex(immediate_value, half_bytes);
      }
      return objcode;
    }
    else if (SYMTAB[temp_operand].exists == 'n')
    {

      if (half_bytes == 3)
      {
        write_data = "Line " + to_string(line_number);
        if (half_bytes == 3)
        {
          write_data += " : Invalid format for external reference " + temp_operand;
        }
        else
        {
          write_data += " : Symbol doesn't exists. Found " + temp_operand;
        }
        write_to_file(error_file, write_data);
        objcode = int_to_string_hex(string_hex_to_int(OPTAB[get_real_opcode(opcode)].opcode) + 1, 2);
        objcode += (half_bytes == 5) ? "100000" : "0000";
        return objcode;
      }
    }
    else
    {
      int operand_address = string_hex_to_int(SYMTAB[temp_operand].address) + string_hex_to_int(BLOCKS[block_num_to_name[SYMTAB[temp_operand].block_number]].start_address);

      /*Process Immediate symbol value*/
      if (half_bytes == 5)
      { /*If format 4*/
        objcode = int_to_string_hex(string_hex_to_int(OPTAB[get_real_opcode(opcode)].opcode) + 1, 2);
        objcode += '1';
        objcode += int_to_string_hex(operand_address, half_bytes);

        /*add modifacation record here*/
        modification_record += "M^" + int_to_string_hex(address + 1, 6) + '^';
        modification_record += (half_bytes == 5) ? "05" : "03";
        modification_record += '\n';

        return objcode;
      }

      /*Handle format 3*/
      program_counter = address + string_hex_to_int(BLOCKS[block_num_to_name[block_number]].start_address);
      program_counter += (half_bytes == 5) ? 4 : 3;
      int relative_address = operand_address - program_counter;

      if (relative_address >= (-2048) && relative_address <= 2047)
      {
        objcode = int_to_string_hex(string_hex_to_int(OPTAB[get_real_opcode(opcode)].opcode) + 1, 2);
        objcode += '2';
        objcode += int_to_string_hex(relative_address, half_bytes);
        return objcode;
      }

      if (!nobase)
      {
        relative_address = operand_address - base_register_value;
        if (relative_address >= 0 && relative_address <= 4095)
        {
          objcode = int_to_string_hex(string_hex_to_int(OPTAB[get_real_opcode(opcode)].opcode) + 1, 2);
          objcode += '4';
          objcode += int_to_string_hex(relative_address, half_bytes);
          return objcode;
        }
      }

      if (operand_address <= 4095)
      {
        objcode = int_to_string_hex(string_hex_to_int(OPTAB[get_real_opcode(opcode)].opcode) + 1, 2);
        objcode += '0';
        objcode += int_to_string_hex(operand_address, half_bytes);

        modification_record += "M^" + int_to_string_hex(address + 1 + string_hex_to_int(BLOCKS[block_num_to_name[block_number]].start_address), 6) + '^';
        modification_record += (half_bytes == 5) ? "05" : "03";
        modification_record += '\n';

        return objcode;
      }
    }
  }
  else if (get_flag_format(operand) == '@')
  {
    string temp_operand = operand.substr(1, operand.length() - 1);
    if (temp_operand.substr(temp_operand.length() - 2, 2) == ",X" || SYMTAB[temp_operand].exists == 'n')
    {
      if (half_bytes == 3)
      {
        write_data = "Line " + to_string(line_number);

        write_data += " : Symbol doesn't exists.Index based addressing not supported with Indirect addressing ";
        write_to_file(error_file, write_data);
        objcode = int_to_string_hex(string_hex_to_int(OPTAB[get_real_opcode(opcode)].opcode) + 2, 2);
        objcode += (half_bytes == 5) ? "100000" : "0000";
        return objcode;
      }
    }
    int operand_address = string_hex_to_int(SYMTAB[temp_operand].address) + string_hex_to_int(BLOCKS[block_num_to_name[SYMTAB[temp_operand].block_number]].start_address);
    program_counter = address + string_hex_to_int(BLOCKS[block_num_to_name[block_number]].start_address);
    program_counter += (half_bytes == 5) ? 4 : 3;

    if (half_bytes == 3)
    {
      int relative_address = operand_address - program_counter;
      if (relative_address >= (-2048) && relative_address <= 2047)
      {
        objcode = int_to_string_hex(string_hex_to_int(OPTAB[get_real_opcode(opcode)].opcode) + 2, 2);
        objcode += '2';
        objcode += int_to_string_hex(relative_address, half_bytes);
        return objcode;
      }

      if (!nobase)
      {
        relative_address = operand_address - base_register_value;
        if (relative_address >= 0 && relative_address <= 4095)
        {
          objcode = int_to_string_hex(string_hex_to_int(OPTAB[get_real_opcode(opcode)].opcode) + 2, 2);
          objcode += '4';
          objcode += int_to_string_hex(relative_address, half_bytes);
          return objcode;
        }
      }

      if (operand_address <= 4095)
      {
        objcode = int_to_string_hex(string_hex_to_int(OPTAB[get_real_opcode(opcode)].opcode) + 2, 2);
        objcode += '0';
        objcode += int_to_string_hex(operand_address, half_bytes);

        modification_record += "M^" + int_to_string_hex(address + 1 + string_hex_to_int(BLOCKS[block_num_to_name[block_number]].start_address), 6) + '^';
        modification_record += (half_bytes == 5) ? "05" : "03";
        modification_record += '\n';

        return objcode;
      }
    }
    else
    {
      objcode = int_to_string_hex(string_hex_to_int(OPTAB[get_real_opcode(opcode)].opcode) + 2, 2);
      objcode += '1';
      objcode += int_to_string_hex(operand_address, half_bytes);

      modification_record += "M^" + int_to_string_hex(address + 1 + string_hex_to_int(BLOCKS[block_num_to_name[block_number]].start_address), 6) + '^';
      modification_record += (half_bytes == 5) ? "05" : "03";
      modification_record += '\n';

      return objcode;
    }

    write_data = "Line: " + to_string(line_number);
    write_data += "Can't fit into program counter based or base register based addressing.";
    write_to_file(error_file, write_data);
    objcode = int_to_string_hex(string_hex_to_int(OPTAB[get_real_opcode(opcode)].opcode) + 2, 2);
    objcode += (half_bytes == 5) ? "100000" : "0000";

    return objcode;
  }
  else if (get_flag_format(operand) == '=')
  {
    string temp_operand = operand.substr(1, operand.length() - 1);
    if (temp_operand == "*")
    {
      temp_operand = "X'" + int_to_string_hex(address, 6) + "'";

      modification_record += "M^" + int_to_string_hex(string_hex_to_int(LITTAB[temp_operand].address) + string_hex_to_int(BLOCKS[block_num_to_name[LITTAB[temp_operand].block_number]].start_address), 6) + '^';
      modification_record += int_to_string_hex(6, 2);
      modification_record += '\n';
    }

    if (LITTAB[temp_operand].exists == 'n')
    {
      write_data = "Line " + to_string(line_number) + " : Symbol doesn't exists. Found " + temp_operand;
      write_to_file(error_file, write_data);

      objcode = int_to_string_hex(string_hex_to_int(OPTAB[get_real_opcode(opcode)].opcode) + 3, 2);
      objcode += (half_bytes == 5) ? "000" : "0";
      objcode += "000";
      return objcode;
    }

    int operand_address = string_hex_to_int(LITTAB[temp_operand].address) + string_hex_to_int(BLOCKS[block_num_to_name[LITTAB[temp_operand].block_number]].start_address);
    program_counter = address + string_hex_to_int(BLOCKS[block_num_to_name[block_number]].start_address);
    program_counter += (half_bytes == 5) ? 4 : 3;

    if (half_bytes == 3)
    {
      int relative_address = operand_address - program_counter;
      if (relative_address >= (-2048) && relative_address <= 2047)
      {
        objcode = int_to_string_hex(string_hex_to_int(OPTAB[get_real_opcode(opcode)].opcode) + 3, 2);
        objcode += '2';
        objcode += int_to_string_hex(relative_address, half_bytes);
        return objcode;
      }

      if (!nobase)
      {
        relative_address = operand_address - base_register_value;
        if (relative_address >= 0 && relative_address <= 4095)
        {
          objcode = int_to_string_hex(string_hex_to_int(OPTAB[get_real_opcode(opcode)].opcode) + 3, 2);
          objcode += '4';
          objcode += int_to_string_hex(relative_address, half_bytes);
          return objcode;
        }
      }

      if (operand_address <= 4095)
      {
        objcode = int_to_string_hex(string_hex_to_int(OPTAB[get_real_opcode(opcode)].opcode) + 3, 2);
        objcode += '0';
        objcode += int_to_string_hex(operand_address, half_bytes);

        modification_record += "M^" + int_to_string_hex(address + 1 + string_hex_to_int(BLOCKS[block_num_to_name[block_number]].start_address), 6) + '^';
        modification_record += (half_bytes == 5) ? "05" : "03";
        modification_record += '\n';

        return objcode;
      }
    }
    else
    {
      objcode = int_to_string_hex(string_hex_to_int(OPTAB[get_real_opcode(opcode)].opcode) + 3, 2);
      objcode += '1';
      objcode += int_to_string_hex(operand_address, half_bytes);

      modification_record += "M^" + int_to_string_hex(address + 1 + string_hex_to_int(BLOCKS[block_num_to_name[block_number]].start_address), 6) + '^';
      modification_record += (half_bytes == 5) ? "05" : "03";
      modification_record += '\n';

      return objcode;
    }

    write_data = "Line: " + to_string(line_number);
    write_data += "Can't fit into program counter based or base register based addressing.";
    write_to_file(error_file, write_data);
    objcode = int_to_string_hex(string_hex_to_int(OPTAB[get_real_opcode(opcode)].opcode) + 3, 2);
    objcode += (half_bytes == 5) ? "100" : "0";
    objcode += "000";

    return objcode;
  }
  else
  {
    int xbpe = 0;
    string temp_operand = operand;
    if (operand.substr(operand.length() - 2, 2) == ",X")
    {
      temp_operand = operand.substr(0, operand.length() - 2);
      xbpe = 8;
    }

    if (SYMTAB[temp_operand].exists == 'n')
    {
      if (half_bytes == 3)
      {

        write_data = "Line " + to_string(line_number);
        write_data += " : Symbol doesn't exists. Found " + temp_operand;
        write_to_file(error_file, write_data);
        objcode = int_to_string_hex(string_hex_to_int(OPTAB[get_real_opcode(opcode)].opcode) + 3, 2);
        objcode += (half_bytes == 5) ? (int_to_string_hex(xbpe + 1, 1) + "00") : int_to_string_hex(xbpe, 1);
        objcode += "000";
        return objcode;
      }
    }
    else
    {

      int operand_address = string_hex_to_int(SYMTAB[temp_operand].address) + string_hex_to_int(BLOCKS[block_num_to_name[SYMTAB[temp_operand].block_number]].start_address);
      program_counter = address + string_hex_to_int(BLOCKS[block_num_to_name[block_number]].start_address);
      program_counter += (half_bytes == 5) ? 4 : 3;

      if (half_bytes == 3)
      {
        int relative_address = operand_address - program_counter;
        if (relative_address >= (-2048) && relative_address <= 2047)
        {
          objcode = int_to_string_hex(string_hex_to_int(OPTAB[get_real_opcode(opcode)].opcode) + 3, 2);
          objcode += int_to_string_hex(xbpe + 2, 1);
          objcode += int_to_string_hex(relative_address, half_bytes);
          return objcode;
        }

        if (!nobase)
        {
          relative_address = operand_address - base_register_value;
          if (relative_address >= 0 && relative_address <= 4095)
          {
            objcode = int_to_string_hex(string_hex_to_int(OPTAB[get_real_opcode(opcode)].opcode) + 3, 2);
            objcode += int_to_string_hex(xbpe + 4, 1);
            objcode += int_to_string_hex(relative_address, half_bytes);
            return objcode;
          }
        }

        if (operand_address <= 4095)
        {
          objcode = int_to_string_hex(string_hex_to_int(OPTAB[get_real_opcode(opcode)].opcode) + 3, 2);
          objcode += int_to_string_hex(xbpe, 1);
          objcode += int_to_string_hex(operand_address, half_bytes);

          modification_record += "M^" + int_to_string_hex(address + 1 + string_hex_to_int(BLOCKS[block_num_to_name[block_number]].start_address), 6) + '^';
          modification_record += (half_bytes == 5) ? "05" : "03";
          modification_record += '\n';

          return objcode;
        }
      }
      else
      {
        objcode = int_to_string_hex(string_hex_to_int(OPTAB[get_real_opcode(opcode)].opcode) + 3, 2);
        objcode += int_to_string_hex(xbpe + 1, 1);
        objcode += int_to_string_hex(operand_address, half_bytes);

        modification_record += "M^" + int_to_string_hex(address + 1 + string_hex_to_int(BLOCKS[block_num_to_name[block_number]].start_address), 6) + '^';
        modification_record += (half_bytes == 5) ? "05" : "03";
        modification_record += '\n';

        return objcode;
      }

      write_data = "Line: " + to_string(line_number);
      write_data += "Can't fit into program counter based or base register based addressing.";
      write_to_file(error_file, write_data);
      objcode = int_to_string_hex(string_hex_to_int(OPTAB[get_real_opcode(opcode)].opcode) + 3, 2);
      objcode += (half_bytes == 5) ? (int_to_string_hex(xbpe + 1, 1) + "00") : int_to_string_hex(xbpe, 1);
      objcode += "000";

      return objcode;
    }
  }
  return "AB";
}

void write_text_record(bool last_record = false)
{
  if (last_record)
  {
    if (current_record.length() > 0)
    {
      write_data = int_to_string_hex(current_record.length() / 2, 2) + '^' + current_record;
      write_to_file(object_file, write_data);
      current_record = "";
    }
    return;
  }
  if (object_code != "")
  {
    if (current_record.length() == 0)
    {
      write_data = "T^" + int_to_string_hex(address + string_hex_to_int(BLOCKS[block_num_to_name[block_number]].start_address), 6) + '^';
      write_to_file(object_file, write_data, false);
    }
    if ((current_record + object_code).length() > 60)
    {
      write_data = int_to_string_hex(current_record.length() / 2, 2) + '^' + current_record;
      write_to_file(object_file, write_data);

      current_record = "";
      write_data = "T^" + int_to_string_hex(address + string_hex_to_int(BLOCKS[block_num_to_name[block_number]].start_address), 6) + '^';
      write_to_file(object_file, write_data, false);
    }

    current_record += object_code;
  }
  else
  {

    if (opcode == "START" || opcode == "END" || opcode == "BASE" || opcode == "NOBASE" || opcode == "LTORG" || opcode == "ORG" || opcode == "EQU")
    {
      /*Assembler directives*/
    }
    else
    {
      if (current_record.length() > 0)
      {
        write_data = int_to_string_hex(current_record.length() / 2, 2) + '^' + current_record;
        write_to_file(object_file, write_data);
      }
      current_record = "";
    }
  }
}

void write_end_record(bool write = true)
{
  if (write)
  {
    if (end_record.length() > 0)
    {
      write_to_file(object_file, end_record);
    }
    else
    {
      write_end_record(false);
    }
  }
  if ((operand == "" || operand == " ") && current_sect_name == "DEFAULT")
  {
    end_record = "E^" + int_to_string_hex(start_address, 6);
  }
  else if (current_sect_name != "DEFAULT")
  {
    end_record = "E";
  }
  else
  {
    int firstExecutableAddress;

    firstExecutableAddress = string_hex_to_int(SYMTAB[first_executable_sec].address);

    end_record = "E^" + int_to_string_hex(firstExecutableAddress, 6) + "\n";
  }
}

void pass2()
{
  string temp_buff;
  intermediate_file.open("intermediate_" + file_name);
  if (!intermediate_file)
  {
    cout << "Unable to open file: intermediate_" << file_name << endl;
    exit(1);
  }
  getline(intermediate_file, temp_buff);

  object_file.open("object_" + file_name);
  if (!object_file)
  {
    cout << "Unable to open file: object_" << file_name << endl;
    exit(1);
  }

  listing_file.open("listing_" + file_name);
  if (!listing_file)
  {
    cout << "Can't to open file: listing_" << file_name << endl;
    exit(1);
  }
  write_to_file(listing_file, "Line\tAddress\tLabel\tOPCODE\tOPERAND\tObjectCode\tComment");

  error_file.open("error_" + file_name, fstream::app);
  if (!error_file)
  {
    cout << "Can't to open file: error_" << file_name << endl;
    exit(1);
  }
  write_to_file(error_file, "\n\n---------------PASS2---------------");

  object_code = "";
  current_text_record_length = 0;
  current_record = "";
  modification_record = "";
  block_number = 0;
  nobase = true;

  read_intermediate_file(intermediate_file, is_comment, line_number, address, block_number, label, opcode, operand, comment);
  while (is_comment)
  {
    write_data = to_string(line_number) + "\t" + comment;
    write_to_file(listing_file, write_data);
    read_intermediate_file(intermediate_file, is_comment, line_number, address, block_number, label, opcode, operand, comment);
  }

  if (opcode == "START")
  {
    start_address = address;
    write_data = to_string(line_number) + "\t" + int_to_string_hex(address) + "\t" + to_string(block_number) + "\t" + label + "\t" + opcode + "\t" + operand + "\t" + object_code + "\t" + comment;
    write_to_file(listing_file, write_data);
  }
  else
  {
    label = "";
    start_address = 0;
    address = 0;
  }

  program_section_length = program_length;

  write_data = "H^" + expand_string(label, 6, ' ', true) + '^' + int_to_string_hex(address, 6) + '^' + int_to_string_hex(program_section_length, 6);
  write_to_file(object_file, write_data);

  read_intermediate_file(intermediate_file, is_comment, line_number, address, block_number, label, opcode, operand, comment);
  current_text_record_length = 0;

  while (opcode != "END")
  {
    while (opcode != "END")
    {
      if (!is_comment)
      {
        if (OPTAB[get_real_opcode(opcode)].exists == 'y')
        {
          if (OPTAB[get_real_opcode(opcode)].format == 1)
          {
            object_code = OPTAB[get_real_opcode(opcode)].opcode;
          }
          else if (OPTAB[get_real_opcode(opcode)].format == 2)
          {
            operand1 = operand.substr(0, operand.find(','));
            operand2 = operand.substr(operand.find(',') + 1, operand.length() - operand.find(',') - 1);

            if (operand2 == operand)
            {
              if (get_real_opcode(opcode) == "SVC")
              {
                object_code = OPTAB[get_real_opcode(opcode)].opcode + int_to_string_hex(stoi(operand1), 1) + '0';
              }
              else if (REGTAB[operand1].exists == 'y')
              {
                object_code = OPTAB[get_real_opcode(opcode)].opcode + REGTAB[operand1].num + '0';
              }
              else
              {
                object_code = get_real_opcode(opcode) + '0' + '0';
                write_data = "Line: " + to_string(line_number) + " Invalid Register name";
                write_to_file(error_file, write_data);
              }
            }
            else
            {
              if (REGTAB[operand1].exists == 'n')
              {
                object_code = OPTAB[get_real_opcode(opcode)].opcode + "00";
                write_data = "Line: " + to_string(line_number) + " Invalid Register name";
                write_to_file(error_file, write_data);
              }
              else if (get_real_opcode(opcode) == "SHIFTR" || get_real_opcode(opcode) == "SHIFTL")
              {
                object_code = OPTAB[get_real_opcode(opcode)].opcode + REGTAB[operand1].num + int_to_string_hex(stoi(operand2), 1);
              }
              else if (REGTAB[operand2].exists == 'n')
              {
                object_code = OPTAB[get_real_opcode(opcode)].opcode + "00";
                write_data = "Line: " + to_string(line_number) + " Invalid Register name";
                write_to_file(error_file, write_data);
              }
              else
              {
                object_code = OPTAB[get_real_opcode(opcode)].opcode + REGTAB[operand1].num + REGTAB[operand2].num;
              }
            }
          }
          else if (OPTAB[get_real_opcode(opcode)].format == 3)
          {
            if (get_real_opcode(opcode) == "RSUB")
            {
              object_code = OPTAB[get_real_opcode(opcode)].opcode;
              object_code += (get_flag_format(opcode) == '+') ? "000000" : "0000";
            }
            else
            {
              object_code = create_object_code_format34();
            }
          }
        }
        else if (opcode == "BYTE")
        {
          if (operand[0] == 'X')
          {
            object_code = operand.substr(2, operand.length() - 3);
          }
          else if (operand[0] == 'C')
          {
            object_code = string_to_hex_string(operand.substr(2, operand.length() - 3));
          }
        }
        else if (label == "*")
        {
          if (opcode[1] == 'C')
          {
            object_code = string_to_hex_string(opcode.substr(3, opcode.length() - 4));
          }
          else if (opcode[1] == 'X')
          {
            object_code = opcode.substr(3, opcode.length() - 4);
          }
        }
        else if (opcode == "WORD")
        {
          object_code = int_to_string_hex(stoi(operand), 6);
        }
        else if (opcode == "BASE")
        {
          if (SYMTAB[operand].exists == 'y')
          {
            base_register_value = string_hex_to_int(SYMTAB[operand].address) + string_hex_to_int(BLOCKS[block_num_to_name[SYMTAB[operand].block_number]].start_address);
            nobase = false;
          }
          else
          {
            write_data = "Line " + to_string(line_number) + " : Symbol doesn't exists. Found " + operand;
            write_to_file(error_file, write_data);
          }
          object_code = "";
        }
        else if (opcode == "NOBASE")
        {
          if (nobase)
          { 
            write_data = "Line " + to_string(line_number) + ": Assembler wasn't using base addressing";
            write_to_file(error_file, write_data);
          }
          else
          {
            nobase = true;
          }
          object_code = "";
        }
        else
        {
          object_code = "";
        }
        write_text_record();
        if (block_number == -1 && address != -1)
        {
          write_data = to_string(line_number) + "\t" + int_to_string_hex(address) + "\t" + " " + "\t" + label + "\t" + opcode + "\t" + operand + "\t" + object_code + "\t" + comment;
        }
        else if (address == -1)
        {
          write_data = to_string(line_number) + "\t" + " " + "\t" + " " + "\t" + label + "\t" + opcode + "\t" + operand + "\t" + object_code + "\t" + comment;
        }

        else
        {
          write_data = to_string(line_number) + "\t" + int_to_string_hex(address) + "\t" + to_string(block_number) + "\t" + label + "\t" + opcode + "\t" + operand + "\t" + object_code + "\t" + comment;
        }
      }
      else
      {
        write_data = to_string(line_number) + "\t" + comment;
      }
      write_to_file(listing_file, write_data);                                                                                    // Write listing line
      read_intermediate_file(intermediate_file, is_comment, line_number, address, block_number, label, opcode, operand, comment); // Read next line
      object_code = "";
    }
    write_text_record();
    write_end_record(false);
  }
  if (opcode != "CSECT")
  {
    while (read_intermediate_file(intermediate_file, is_comment, line_number, address, block_number, label, opcode, operand, comment))
    {
      if (label == "*")
      {
        if (opcode[1] == 'C')
        {
          object_code = string_to_hex_string(opcode.substr(3, opcode.length() - 4));
        }
        else if (opcode[1] == 'X')
        {
          object_code = opcode.substr(3, opcode.length() - 4);
        }
        write_text_record();
      }
      write_data = to_string(line_number) + "\t" + int_to_string_hex(address) + "\t" + to_string(block_number) + label + "\t" + opcode + "\t" + operand + "\t" + object_code + "\t" + comment;
      write_to_file(listing_file, write_data);
    }
  }

  write_text_record(true);
  if (!is_comment)
  {

    write_to_file(object_file, modification_record, false);
    write_end_record(true);
    current_sect_name = label;
    modification_record = "";
  }
  read_intermediate_file(intermediate_file, is_comment, line_number, address, block_number, label, opcode, operand, comment); // Read next line
  object_code = "";
}
