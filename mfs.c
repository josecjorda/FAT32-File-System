// The MIT License (MIT)
// 
// Copyright (c) 2020 Trevor Bakker 
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#define _GNU_SOURCE

#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <stdbool.h>
#include <ctype.h>
#include <stdint.h>

FILE *fp;
struct __attribute__((__packed__)) DirectoryEntry{
  char DIR_NAME[11];
  uint8_t DIR_Attr;
  uint8_t Unused1[8];
  uint16_t DIR_FirstClusterHigh;
  uint8_t Unused2[4];
  uint16_t DIR_FirstClusterLow;
  uint32_t DIR_FileSize;
};
struct DirectoryEntry dir[16];

char BS_OEMName[8];
int16_t BPB_BytsPerSec;
int8_t BPB_SecPerClus;
int16_t BPB_RsvdSecCnt;
int8_t BPB_NumFATS;
int16_t BPB_RootEntCnt;
char BS_VolLab[11];
int32_t BPB_FATSz32;
int32_t BPB_RootClus;

int32_t RootDirSectors = 0;
int32_t FirstDataSector = 0;
int32_t FirstSectorofCluster = 0;


int LBAToOffset(int32_t sector){
  return ((sector-2)*BPB_BytsPerSec) + (BPB_BytsPerSec*BPB_RsvdSecCnt) + (BPB_NumFATS*BPB_FATSz32*BPB_BytsPerSec);
}

int16_t NextLB( uint32_t sector)
{
  uint32_t FATAddress = ( BPB_BytsPerSec * BPB_RsvdSecCnt) + ( sector * 4);
  int16_t val;
  fseek( fp, FATAddress,SEEK_SET);
  fread( &val, 2, 1, fp);
  return val;
}
#define MAX_NUM_ARGUMENTS 3

#define WHITESPACE " \t\n"      // We want to split our command line up into tokens
                                // so we need to define what delimits our tokens.
                                // In this case  white space
                                // will separate the tokens on our command line

#define MAX_COMMAND_SIZE 255    // The maximum command-line size


bool compare(char * name, char * change)
{
  bool tf = false;

  char IMG_Name[12];
  strncpy(IMG_Name, name,11);

  char input[12];
  strncpy(input, change,11);


  char expanded_name[12];
  if(strncmp(change,"..",2) != 0)
  {
    memset( expanded_name, ' ', 12 );

    char *token = strtok( input, "." );
    strncpy( expanded_name, token, strlen( token ) );

    token = strtok( NULL, "." );

    if( token )
    {
      strncpy( (char*)(expanded_name+8), token, strlen(token ) );
    }

    expanded_name[11] = '\0';

    int i;
    for( i = 0; i < 11; i++ )
    {
      expanded_name[i] = toupper( expanded_name[i] );
    }
  }
  else
  {
    strncpy(expanded_name, "..",2);
    expanded_name[3] = '\0';
    if( strncmp( expanded_name, IMG_Name, 2 ) == 0 )
    {
      tf = true;
    }
    return tf;
  }
  if( strncmp( expanded_name, IMG_Name, 11 ) == 0 )
  {
    tf = true;
  }
  return tf;
}




int main()
{

  char * cmd_str = (char*) malloc( MAX_COMMAND_SIZE );
  while( 1 )
  {
    // Print out the mfs prompt
    printf ("mfs> ");

    // Read the command from the commandline.  The
    // maximum command that will be read is MAX_COMMAND_SIZE
    // This while command will wait here until the user
    // inputs something since fgets returns NULL when there
    // is no input
    while( !fgets (cmd_str, MAX_COMMAND_SIZE, stdin) );
    /* Parse input */
    char *token[MAX_NUM_ARGUMENTS];

    int   token_count = 0;                                 
    char firstletter;                                       
    // Pointer to point to the token
    // parsed by strsep
    char *arg_ptr;                                         
                                                           
    char *working_str  = strdup( cmd_str );                

    // we are going to move the working_str pointer so
    // keep track of its original value so we can deallocate
    // the correct amount at the end
    char *working_root = working_str;

    // Tokenize the input stringswith whitespace used as the delimiter
    while ( ( (arg_ptr = strsep(&working_str, WHITESPACE ) ) != NULL) && 
              (token_count<MAX_NUM_ARGUMENTS))
    {
      token[token_count] = strndup( arg_ptr, MAX_COMMAND_SIZE );
      if( strlen( token[token_count] ) == 0 )
      {
        token[token_count] = NULL;
      }
        token_count++;
    }

    // Now print the tokenized input as a debug check
    // \TODO Remove this code and replace with your FAT32 functionality


    if(strcmp(token[0],"quit") == 0 || strcmp(token[0],"exit") == 0)
    {
      exit(0);
    }
    else if(strcmp(token[0],"open") == 0)
    {
      if(fp== NULL)
      {
        fp = fopen(token[1],"r");  
        if(fp == NULL)
        {
          printf("Error: File system image not found\n");
        }
        else
        {
          fseek( fp,11,SEEK_SET);
          fread(&BPB_BytsPerSec,2,1,fp);

          fseek( fp,13,SEEK_SET);
          fread(&BPB_SecPerClus,1,1,fp);
          
          fseek( fp,14,SEEK_SET);
          fread(&BPB_RsvdSecCnt,2,1,fp);

          fseek( fp,16,SEEK_SET);
          fread(&BPB_NumFATS,1,1,fp);
          
          fseek( fp,36,SEEK_SET);
          fread(&BPB_FATSz32,4,1,fp);

          //this is cd but its hardcoded
          fseek( fp,0x100400,SEEK_SET);
          fread(&dir[0], sizeof(struct DirectoryEntry), 16, fp);

        }
      }
      else
      {
        printf("Error: File system image already open\n");
      }
    }
    else if(strcmp(token[0],"close") == 0)
    {
      if(fp==NULL)
      {
        printf("Error: File system not open\n");
      }
      else
      {
        fclose(fp);
        fp = NULL;
      }
    }
    else if(fp == NULL)
    {
      printf("Error: File system image must be opened first.\n");
    }
    else if(strcmp(token[0],"info") == 0)//need to convert to hexadecimal
    {
      printf("BPB_BytsPerSec: %d\n", BPB_BytsPerSec);
      printf("BPB_SecPerClus: %d\n", BPB_SecPerClus);
      printf("BPB_RsvdSecCnt: %d\n", BPB_RsvdSecCnt);
      printf("BPB_NumFATS: %d\n", BPB_NumFATS);
      printf("BPB_FATSz32: %d\n", BPB_FATSz32);
    }
    else if(strcmp(token[0],"stat") == 0)//not sure about the constraints he listed
    {
      for(int x = 0; x<16;x++)
      {
        char filename[11];
        memcpy(filename,dir[x].DIR_NAME,11);
        filename[11] = '\0';
        printf("FileName: %s at cluster: %d with size: %d and attribute: %d\n",filename,dir[x].DIR_FirstClusterLow,dir[x].DIR_FileSize,dir[x].DIR_Attr);
      }
    }
    else if(strcmp(token[0],"get") == 0)//code from class
    {
      for(int x = 0; x<16;x++)
      {
        if(compare(dir[x].DIR_NAME, token[1]) == true)
        {
          int cluster = dir[x].DIR_FirstClusterLow;
          int offset = LBAToOffset(cluster);
          int size = dir[x].DIR_FileSize;
          fseek(fp,offset,SEEK_SET);
          FILE *ofp = fopen(token[1],"w");
          uint8_t buffer[512];
          while(size >= BPB_BytsPerSec)
          {
            fread(buffer,512,1,fp);
            fwrite(buffer,512,1,ofp);
            size = size-512;
            cluster = NextLB(cluster);
            offset = LBAToOffset(cluster);
            fseek(fp,offset,SEEK_SET);
          }
          if(size>0)
          {
            fread(buffer,size,1,fp);
            fwrite(buffer,size,1,ofp);
            fclose(ofp);
          }
          else
          {
            fclose(ofp);
          }
        }
      }
    }
    else if(strcmp(token[0],"cd") == 0)
    {
      for(int x = 0; x<16;x++)
      {
        if(compare(dir[x].DIR_NAME, token[1]) == true)
        {
          int offset;
          if(dir[x].DIR_FirstClusterLow == 0)//if at starting cluster
          {
            offset = LBAToOffset(2); 
          }
          else
          {
            offset = LBAToOffset(dir[x].DIR_FirstClusterLow);
          }
      
          fseek(fp,offset,SEEK_SET);
          fread(dir,sizeof(struct DirectoryEntry),16,fp);
        }
      }
    }
    else if(strcmp(token[0],"ls") == 0)
    {
      for(int x = 0; x<16;x++)
      {
        if(dir[x].DIR_NAME[0] != 0xe5 && (dir[x].DIR_Attr== 0x01 ||dir[x].DIR_Attr== 0x10 || dir[x].DIR_Attr== 0x20))
        {
          char filename[11];
          memcpy(filename,dir[x].DIR_NAME,11);
          filename[11] = '\0';
          printf("%s \n",filename);
        }
      }
    }
    else if(strcmp(token[0],"read") == 0)
    {
      for(int x = 0; x<16;x++)
      {
        if(compare(dir[x].DIR_NAME, token[1]) == true)
        {
          uint8_t temp[512];
          int offset = LBAToOffset(dir[x].DIR_FirstClusterLow);
          fseek(fp,offset,SEEK_SET);
          fseek(fp,atoi(token[2]),SEEK_CUR);
          fread(temp,atoi(token[3]),1,fp);

          for(int y =0;y<atoi(token[3]);y++)
          {
            printf("%d ", temp[y]);
          }
          printf("\n");
        }
      }
    }
    else if(strcmp(token[0],"del") == 0)
    {
      for(int x = 0; x<16;x++)
      {
        if(compare(dir[x].DIR_NAME, token[1]) == true)
        {
          firstletter = dir[x].DIR_NAME[0];
          //wasnt sure how to use 0xe5 so used 1 as a replacement
          dir[x].DIR_NAME[0] = '1';
          //attr causes it to dissapear
          dir[x].DIR_Attr = 0;
        }
      }
    }
    else if(strcmp(token[0],"undel") == 0)
    {
      for(int x = 0; x<16;x++)
      {
        if(dir[x].DIR_NAME[0] =='1')
        {
          dir[x].DIR_NAME[0] = firstletter;
          //attr causes it to reappear
          dir[x].DIR_Attr = 0x1;
        }
      }
    }
    

    free( working_root );

  }
  return 0;
}
