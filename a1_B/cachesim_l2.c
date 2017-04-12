#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

long int hexToDec(char* cyc_num);
void hexToBin(char* address, char* hex_addr);
void fill_bin_address(char* address, int* fill_trace, char* bin_num);

int main(int argc, char *argv[])
{
  char* config_file_name = argv[1];
  char* trace_file_name = argv[2];

  FILE* fp_config;
  FILE* fp_trace;
  /*contains the setting for l1 cache*/
  /*0 is for l1 capacity */
  /*1 is for l1 association*/
  /*2 is for l1 cacheline size*/
  /*3 is for l1 hit latency*/
  /*4 is for l2 hit latency*/
  /*5 is for main memory latency*/
  /*6 is for l2 capacity*/
  /*7 is for l2 association*/
  /*8 is for l2 cacheline size*/
  int setting[9];  

  /*temperary variables*/
  char buff[255];
  char *data; /*This is for the below while loop to store either text or number*/ 
  int i = 0;
  const char s[2] = ":";

  /*open the configuration file and read in the settings for the l1 cache*/  
  fp_config = fopen(config_file_name, "r");
  while(fscanf(fp_config, "%s", buff) == 1)
  {
    data = strtok(buff, s);
    while(data != NULL)
    {
      if(data[0] != 'l' && data[0] != 'm') {setting[i++] = atoi(data); break;}
      data = strtok(NULL, s);
    }
  }  
 
  /*construct the fake cache for simulating*/  
  int cacheline_num = (setting[0] * 1024) / setting[2];
  int index_num = log(cacheline_num / setting[1])/log(2); /*compute how many bits in the address are needed for the index*/
  int offset_num = log(setting[2])/log(2); /*compute how many bits in the address are needed for the offset*/
  int tag_num = 64 - index_num - offset_num;
 
  int cacheline_num_l2 = (setting[6] * 1024 * 1024) / setting[8];
  int index_num_l2 = log(cacheline_num_l2 / setting[7])/log(2); /*compute how many bits in the address are needed for the index*/
  int offset_num_l2 = log(setting[8])/log(2); /*compute how many bits in the address are needed for the offset*/
  int tag_num_l2 = 64 - index_num_l2 - offset_num_l2;

  /*The Cache Block*/
  /*all to be initialized as 0*/
  /*each will hold valid bit and dirty bit, so that the value will be:*/
  /*vd = 00 = 0*/
  /*vd = 01(meaningless) = 1 = 0*/
  /*vd = 10*/
  /*vd = 11*/

  int* cache_block = malloc(sizeof(int) * cacheline_num);
  for(i = 0; i < cacheline_num; i ++)
  {
    cache_block[i] = 0;
  }
 
  int* cache_block_l2 = malloc(sizeof(int) * cacheline_num_l2);
  for(i = 0; i < cacheline_num_l2; i ++)
  {
    cache_block_l2[i] = 0;
  }  

  /*This array stores the cycle the block is accessed*/
  int* acc_cycle = malloc(sizeof(int) * cacheline_num);
  int* acc_cycle_l2 = malloc(sizeof(int) * cacheline_num_l2);
  /*This array stores each cache line's corresponding tag number*/
  char** tag_block = malloc(sizeof(char*) * cacheline_num);
  char** tag_block_l2 = malloc(sizeof(char*) * cacheline_num_l2);

  for(i = 0; i < cacheline_num; i ++)
  {
    tag_block[i] = NULL;
    acc_cycle[i] = 0;
  }

  for(i = 0; i < cacheline_num_l2; i++)
  {
    tag_block_l2[i] = NULL;
    acc_cycle_l2[i] = 0;
  }

  /*take in the trace file and begin tracing*/
  /*trace_input contains cycle number,read or write number,address*/
  char* trace_input = malloc(sizeof(char)); 
  char* input_splitted[3]; /*container for the 3 inputs from trace*/
  const char split[2] = ","; /*Used for splitting the string*/

  /*For tracking and final output*/
  int l1_hit = 0;
  int l2_hit = 0;
  int l1_reference = 0; /*l1_reference is just the total reference to the memory system*/
  int l2_reference = 0;
  int total_latency = 0;
  double hit_rate;
  double hit_rate_l2;
  double AMAT;  

  /*For serving the current address*/
  long int cycle_num;
  char address[64];
  char* tag = malloc(sizeof(char) * (tag_num + 1)); /*The tag of the current address*/
  char* tag_l2 = malloc(sizeof(char) * (tag_num_l2 + 1)); 
 
  /*Below values needs to be reinitialized every cycle*/
  int index = 0; /*The index of the current address*/
  int index_l2 = 0;
  int offset = 0;
  int offset_l2 = 0;
  int line_pointer = 0; /*Used when going line by line in the cache to compare tag, offset and index*/ 
  int line_pointer_l2 = 0;
  int miss; /*A boolean varialbe, 1 means miss, 0 means hit*/
  int miss_l2;
  int line_cycle_num; /*when miss, record each line's accessed cycle number when comparing and deciding which one to evict*/
  int line_cycle_num_l2;
  int replace_line = -1; /*when miss, as the line to be replaced by if any line is empty or invalid, -1 means undetermined*/   
  int replace_line_l2 = -1;
  int evict_line = -1; /*when miss, as the line to be evicted if all lines within set are occupied, -1 means undetermined*/
  int evict_line_l2 = -1;

  memset(address, '0', 64); /*initialize all first 64 characters to be '0'*/
  memset(tag, '0', tag_num+1);
  memset(tag_l2, '0', tag_num_l2+1);
   
  fp_trace = fopen(trace_file_name, "r");
  while(fscanf(fp_trace, "%s", trace_input) == 1)
  {
    /*No matter what, you will access the l1 anyway*/
    l1_reference ++;

    /*reinitialization*/
    index = 0;
    index_l2 = 0;
    offset = 0;
    offset_l2 = 0;
    line_pointer = 0;
    line_pointer_l2 = 0;
    miss = 1; /*presume this is a miss request*/
    miss_l2 = 1;
    line_cycle_num = 0;
    line_cycle_num_l2 = 0;
    replace_line = -1;
    replace_line_l2 = -1;
    evict_line = -1;
    evict_line_l2 = -1;

    /*Split the trace input into 3 pieces: cycle number, R/W, and address*/
    for(i = 0; i < 3; i ++)
    {
      if(i == 0)
      {
        input_splitted[i] = strtok(trace_input, split);
      }
      else
      {
        input_splitted[i] = strtok(NULL, split);
      }
    }
    
    /*now we have the current cycle number in decimal*/
    cycle_num = hexToDec(input_splitted[0]);
    /*now we have an binary address string converted from the hex address from trace file*/
    hexToBin(address, input_splitted[2]);
    /*First extract the offset number from the binary address, store it as int*/
    for(i = 63; i > (63 - offset_num); i --)
    {
      offset += (address[i] - 48) * (int)pow(2.0, (double)(63-i)); 
    }
    for(i = 63; i > (63 - offset_num_l2); i --)
    {
      offset_l2 += (address[i] - 48) * (int)pow(2.0, (double)(63-i)); 
    }

    /*Then extract the index number from the binary address, store it as int*/
    for(i = (63 - offset_num); i > (63 - offset_num - index_num); i --)
    {
      index += (address[i] - 48) * (int)pow(2.0, (double)((63 - offset_num) - i));
    }
    for(i = (63 - offset_num_l2); i > (63 - offset_num_l2 - index_num_l2); i --)
    {
      index_l2 += (address[i] - 48) * (int)pow(2.0, (double)((63 - offset_num_l2) - i));
    }
    /*Finally extract the tag number from the binary address*/
    for(i = (63 - offset_num - index_num); i >=0; i --)
    {
      tag[i] = address[i];
    }
    tag[tag_num] = '\0'; /*Null terminates*/
    for(i = (63 - offset_num_l2 - index_num_l2); i >=0; i --)
    {
      tag_l2[i] = address[i];
    }
    tag_l2[tag_num_l2] = '\0'; /*Null terminates*/

    /*Try to see if hit*/
    for(i = 0; i < setting[1]; i++)
    { 
      /*line_pointer will point to each cache_line within the set located in the cache*/
      line_pointer = (setting[1] * index) + i; 
      if(tag_block[line_pointer] != NULL)
      {
        /*hit*/
 	if(strcmp(tag_block[line_pointer], tag) == 0)	
        {
          /*hit & valid*/
	  if(cache_block[line_pointer] != 0)
	  {	
	    miss = 0;
	    l1_hit ++;  
            if(strcmp(input_splitted[1], "0") == 0)
	    {
	      /*if write hit, update the dirty bit*/
	      cache_block[line_pointer] = 11;
            }
	    total_latency += setting[3];
	    acc_cycle[line_pointer] = cycle_num;
	    break; 
	  } 
        }
        /*Otherwise conditionally update the LRU and its corresponding accessed cycle number*/
	else 
	{
	  if(line_cycle_num == 0)
	  {
	    line_cycle_num = acc_cycle[line_pointer];
	    evict_line = line_pointer;
	  }
	  else 
	  {
	    if(acc_cycle[line_pointer] < line_cycle_num)
	    {
	      /*now this line may be the potential evicted line according to LRU*/
	      evict_line = line_pointer;
	      line_cycle_num = acc_cycle[line_pointer];
	    }
	  }
	}
      } 
      /*maybe this request is miss, and need to record something for miss service*/
      else
      {  
	/*if this cache line is empty or not valid, record it*/
	if((tag_block[line_pointer] == NULL || cache_block[line_pointer] == 0) && replace_line == -1)
	{
	  replace_line = line_pointer; /*This line may be the potential replaced line if miss*/
	} 
      }
    }
     
    /*l1_miss*/      
    if(miss)
    {
      l2_reference ++;
      /*Try to see if hit in l2*/
      for(i = 0; i < setting[7]; i++)
      { 
        /*line_pointer will point to each cache_line within the set located in the cache*/
        line_pointer_l2 = (setting[7] * index_l2) + i; 
        if(tag_block_l2[line_pointer_l2] != NULL)
        {
          /*hit*/
 	  if(strcmp(tag_block_l2[line_pointer_l2], tag_l2) == 0)	
          {
            /*hit & valid*/
	    if(cache_block_l2[line_pointer_l2] != 0)
	    {	
	      miss_l2 = 0;
	      l2_hit ++;  
              if(strcmp(input_splitted[1], "0") == 0)
	      {
	        /*if write hit, update the dirty bit*/
	        cache_block_l2[line_pointer_l2] = 11;
              }
	      total_latency += (setting[4] + setting[3]);
	      acc_cycle_l2[line_pointer_l2] = cycle_num;
              
	      /*Bring back to L1*/
              if(replace_line != -1)
              { 
        	cache_block[replace_line] = 10;
        	tag_block[replace_line] = malloc(sizeof(char) * (tag_num + 1));
		memcpy(tag_block[replace_line], tag, tag_num + 1);
		acc_cycle[replace_line] = cycle_num; 
              }
      	      /*not dirty, just replace the least used one*/
      	      else if(cache_block[evict_line] == 10)
              {
		acc_cycle[evict_line] = cycle_num; 
    		tag_block[evict_line] = malloc(sizeof(char) * (tag_num + 1));
		memcpy(tag_block[evict_line], tag, tag_num + 1);
      	      }
      	      /*dirty, need write back and replace the least used one*/
      	      else
      	      {
		cache_block[evict_line] = 10;
		acc_cycle[evict_line] = cycle_num; 
    		tag_block[evict_line] = malloc(sizeof(char) * (tag_num + 1));
		memcpy(tag_block[evict_line], tag, tag_num + 1);
      	      }
	      break; 
	    } 
          }
          /*Otherwise conditionally update the LRU and its corresponding accessed cycle number*/
	  else 
	  {
	    if(line_cycle_num_l2 == 0)
	    {
	      line_cycle_num_l2 = acc_cycle_l2[line_pointer_l2];
	      evict_line_l2 = line_pointer_l2;
	    }
	    else 
	    {
	      if(acc_cycle_l2[line_pointer_l2] < line_cycle_num_l2)
	      {
	        /*now this line may be the potential evicted line according to LRU*/
	        evict_line_l2 = line_pointer_l2;
	        line_cycle_num_l2 = acc_cycle_l2[line_pointer_l2];
	      }
	    }
	  }
        }   
        /*maybe this request is miss, and need to record something for miss service*/
        else
        {  
	  /*if this cache line is empty or not valid, record it*/
	  if((tag_block_l2[line_pointer_l2] == NULL || cache_block_l2[line_pointer_l2] == 0) && replace_line_l2 == -1)
	  {
	    replace_line_l2 = line_pointer_l2; /*This line may be the potential replaced line if miss*/
	  } 
        }
      } 
    }

    /*But if both l1 and l2 miss*/	
    if(miss && miss_l2)
    {
      total_latency += (setting[5] + setting[4] + setting[3]); 
      /*Need to write to L2*/
      if(replace_line_l2 != -1)
      { 
        cache_block_l2[replace_line_l2] = 10;
        tag_block_l2[replace_line_l2] = malloc(sizeof(char) * (tag_num_l2 + 1));
	memcpy(tag_block_l2[replace_line_l2], tag_l2, tag_num_l2+1);
	acc_cycle_l2[replace_line_l2] = cycle_num; 
      }
      /*not dirty, just replace the least used one*/
      else if(cache_block_l2[evict_line_l2] == 10)
      {
	acc_cycle_l2[evict_line_l2] = cycle_num; 
    	tag_block_l2[evict_line_l2] = malloc(sizeof(char) * (tag_num_l2 + 1));
	memcpy(tag_block_l2[evict_line_l2], tag_l2, tag_num_l2+1);
      }
      /*dirty, need write back and replace the least used one*/
      else
      {
	cache_block_l2[evict_line_l2] = 10;
	acc_cycle_l2[evict_line_l2] = cycle_num; 
    	tag_block_l2[evict_line_l2] = malloc(sizeof(char) * tag_num_l2 + 1);
	memcpy(tag_block_l2[evict_line_l2], tag_l2, tag_num_l2+1);
      }

      /*Need to write to L1*/
      if(replace_line != -1)
      { 
        cache_block[replace_line] = 10;
        tag_block[replace_line] = malloc(sizeof(char) * (tag_num+1));
	memcpy(tag_block[replace_line], tag, tag_num+1);
	acc_cycle[replace_line] = cycle_num; 
      }
      /*not dirty, just replace the least used one*/
      else if(cache_block[evict_line] == 10)
      {
	acc_cycle[evict_line] = cycle_num; 
    	tag_block[evict_line] = malloc(sizeof(char) * (tag_num+1));
	memcpy(tag_block[evict_line], tag, tag_num+1);
      }
      /*dirty, need write back and replace the least used one*/
      else
      {
	cache_block[evict_line] = 10;
	acc_cycle[evict_line] = cycle_num; 
    	tag_block[evict_line] = malloc(sizeof(char) * (tag_num+1));
	memcpy(tag_block[evict_line], tag, tag_num+1);
      }
    }          
  }

  /*computing*/
  hit_rate = ((double)l1_hit) / ((double)l1_reference);
  hit_rate_l2 = ((double)l2_hit) / ((double)l2_reference);
  AMAT = (double)(total_latency) / (double)(l1_reference); /*l1_reference is just the total number of reference to the memory system*/
  printf("%%cachesim_l1...\n");
  printf("L1 hit rate: %.2f\n", hit_rate);
  printf("L2 hit rate: %.2f\n", hit_rate_l2);
  printf("Total latency: %d\n", total_latency);
  printf("L1 references: %d\n", l1_reference);
  printf("L2 references: %d\n", l2_reference);
  printf("AMAT: %.2f\n", AMAT);
  printf("%%\n");

  /*Wrap up*/
  for(i = 0; i < cacheline_num; i ++)
  {
    if(tag_block[i] != NULL)
    {
      free(tag_block[i]); 
    }
  } 
  free(cache_block);
  free(tag_block);
  free(tag);
  free(trace_input);
  free(acc_cycle);
  for(i = 0; i < cacheline_num_l2; i ++)
  {
    if(tag_block_l2[i] != NULL)
    {
      free(tag_block_l2[i]); 
    }
  } 
  free(cache_block_l2);
  free(tag_block_l2);
  free(tag_l2);
  free(acc_cycle_l2);

  return 0;   
}

long int hexToDec(char* cyc_num)
{
  long int ret = 0; /*contains the converted integer*/
  int i;
  
  for(i = strlen(cyc_num) - 1; i >= 0; i --)
  { 
    /*0 - 9, 57 is the ASCII dec for 9*/
    if(cyc_num[i] <= 57)
    {
      ret += (cyc_num[i] - 48) * (int)pow(16.0, (double)(strlen(cyc_num) - 1 - i));
    } 
    /*A - F, 65 is the ASCII dec for A*/
    else if(cyc_num[i] >= 65 && cyc_num[i] <= 70)
    {
      ret += (cyc_num[i] - 55) * (int)pow(16.0, (double)(strlen(cyc_num) - 1 - i));
    }
    /*a - f, 97 is the ASCII dec for a*/
    else
    {
      ret += (cyc_num[i] - 87) * (int)pow(16.0, (double)(strlen(cyc_num) - 1 - i));
    }
  }

  return ret;  
}

void hexToBin(char* address, char* hex_addr)
{
  int i;
  int fill_trace = 63; /*trace the filling in process for the converted binary address string*/

  for(i = strlen(hex_addr) - 1; i >= 0; i --) 
  {
    switch(hex_addr[i])
    {
      case '0': 
          fill_bin_address(address, &fill_trace, "0000");
          break;
      case '1': 
          fill_bin_address(address, &fill_trace, "0001");
          break;
      case '2': 
          fill_bin_address(address, &fill_trace, "0010");
          break;
      case '3': 
          fill_bin_address(address, &fill_trace, "0011");      
          break;
      case '4': 
          fill_bin_address(address, &fill_trace, "0100");
          break;
      case '5': 
          fill_bin_address(address, &fill_trace, "0101");
          break;
      case '6': 
          fill_bin_address(address, &fill_trace, "0110");
          break;
      case '7': 
          fill_bin_address(address, &fill_trace, "0111");      
          break;
      case '8': 
          fill_bin_address(address, &fill_trace, "1000");
          break;
      case '9': 
          fill_bin_address(address, &fill_trace, "1001");
          break;
      case 'A': case 'a':
          fill_bin_address(address, &fill_trace, "1010");
          break;
      case 'B': case 'b':
          fill_bin_address(address, &fill_trace, "1011");      
          break;
      case 'C': case 'c':
          fill_bin_address(address, &fill_trace, "1100");
          break;
      case 'D': case 'd': 
          fill_bin_address(address, &fill_trace, "1101");
          break;
      case 'E': case 'e': 
          fill_bin_address(address, &fill_trace, "1110");
          break;
      case 'F': case 'f':
          fill_bin_address(address, &fill_trace, "1111");      
          break;
    } 
  }

  /*FIll the other unfilled address bits to be 0*/
  for(i = fill_trace; i >= 0; i --)
  {
    address[i] = '0'; 
  }   
}

/*just help filling in the binary number into the corresponding position in the binary address string*/
void fill_bin_address(char* address, int* fill_trace, char* bin_num)
{
  address[(*fill_trace)--] = bin_num[3];
  address[(*fill_trace)--] = bin_num[2];
  address[(*fill_trace)--] = bin_num[1];
  address[(*fill_trace)--] = bin_num[0];  
}
