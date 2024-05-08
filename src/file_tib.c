/*

    File: file_tib.c

    Copyright (C) 2007-2010 Christophe GRENIER <grenier@cgsecurity.org>
  
    This software is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
  
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
  
    You should have received a copy of the GNU General Public License along
    with this program; if not, write the Free Software Foundation, Inc., 51
    Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

 */

#if !defined(SINGLE_FORMAT) || defined(SINGLE_FORMAT_tib)
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#include <stdio.h>
#include "types.h"
#include "filegen.h"
#include "common.h"

/*@ requires valid_register_header_check(file_stat); */
static void register_header_check_tib(file_stat_t *file_stat);

const file_hint_t file_hint_tib= {
  .extension="tib",
  .description="Acronis True Image",
  .max_filesize=PHOTOREC_MAX_FILE_SIZE,
  .recover=1,
  .enable_by_default=1,
  .register_header_check=&register_header_check_tib
};

static const unsigned char tib2_footer[7]= {0x00, 0x00, 0x20, 0xa2, 0xb9, 0x24, 0xce};

/*@
  @ requires file_recovery->data_check==&data_check_tib2;
  @ requires valid_data_check_param(buffer, buffer_size, file_recovery);
  @ ensures  valid_data_check_result(\result, file_recovery);
  @ assigns file_recovery->calculated_file_size;
  @*/
static data_check_t data_check_tib2(const unsigned char *buffer, const unsigned int buffer_size, file_recovery_t *file_recovery)
{
  /*@ assert file_recovery->calculated_file_size <= PHOTOREC_MAX_FILE_SIZE; */
  /*@ assert file_recovery->file_size <= PHOTOREC_MAX_FILE_SIZE; */
  /*@
    @ loop assigns file_recovery->calculated_file_size;
    @ loop variant file_recovery->file_size + buffer_size/2 - (file_recovery->calculated_file_size + 512);
    @*/
  while(file_recovery->calculated_file_size + buffer_size/2  >= file_recovery->file_size &&
      file_recovery->calculated_file_size + 512 <= file_recovery->file_size + buffer_size/2)
  {
    const unsigned int i=file_recovery->calculated_file_size + buffer_size/2 - file_recovery->file_size;
    /*@ assert 0 <= i <= buffer_size - 512; */
    file_recovery->calculated_file_size+=512;
    if(memcmp(&buffer[i + 512 - sizeof(tib2_footer)], tib2_footer, sizeof(tib2_footer))==0)
      return DC_STOP;
  }
  return DC_CONTINUE;
}

/*@
  @ requires file_recovery->file_check == &file_check_tib2;
  @ requires valid_file_check_param(file_recovery);
  @ ensures  valid_file_check_result(file_recovery);
  @ assigns *file_recovery->handle, errno, file_recovery->file_size;
  @ assigns Frama_C_entropy_source;
  @*/
static void file_check_tib2(file_recovery_t *file_recovery)
{
  int64_t file_size;
  if(file_recovery->calculated_file_size < 512)
  {
    file_recovery->file_size=0;
    return ;
  }
  file_size=file_recovery->calculated_file_size-512;
  file_recovery->file_size = file_recovery->calculated_file_size;
  {
    char buffer[512];
    if(my_fseek(file_recovery->handle, file_size, SEEK_SET) < 0 ||
	fread(buffer, 1, 512, file_recovery->handle) != 512)
    {
      file_recovery->file_size=0;
      return;
    }
#ifdef __FRAMAC__
    Frama_C_make_unknown(buffer, 512);
#endif
    if(memcmp(&buffer[512 - sizeof(tib2_footer)], tib2_footer, sizeof(tib2_footer))==0)
    {
      return;
    }
  }

/*@
  @ loop assigns *file_recovery->handle, errno, file_size;
  @ loop assigns Frama_C_entropy_source;
  @ loop variant file_size;
  @*/
  for(; file_size>0; file_size-=512)
  {
    unsigned int i;
    char buffer[512];
    if(my_fseek(file_recovery->handle, file_size, SEEK_SET) < 0 ||
	fread(buffer, 1, 512, file_recovery->handle) != 512)
    {
      file_recovery->file_size=0;
      return;
    }
    /*@
      @ loop assigns i;
      @ loop variant 512 - i;
      @*/
    for(i=0; i<512 && buffer[i]==0; i++);
    if(i!=512)
    {
      file_recovery->file_size=file_size + 512;
      return ;
    }
  }
}

/*@
  @ requires separation: \separated(&file_hint_tib, buffer+(..), file_recovery, file_recovery_new);
  @ requires valid_header_check_param(buffer, buffer_size, safe_header_only, file_recovery, file_recovery_new);
  @ terminates \true;
  @ ensures  valid_header_check_result(\result, file_recovery_new);
  @ assigns  *file_recovery_new;
  @*/
static int header_check_tib(const unsigned char *buffer, const unsigned int buffer_size, const unsigned int safe_header_only, const file_recovery_t *file_recovery, file_recovery_t *file_recovery_new)
{
  reset_file_recovery(file_recovery_new);
  file_recovery_new->extension=file_hint_tib.extension;
  return 1;
}

/*@
  @ requires separation: \separated(&file_hint_tib, buffer+(..), file_recovery, file_recovery_new);
  @ requires valid_header_check_param(buffer, buffer_size, safe_header_only, file_recovery, file_recovery_new);
  @ terminates \true;
  @ ensures  valid_header_check_result(\result, file_recovery_new);
  @ assigns  *file_recovery_new;
  @*/
static int header_check_tib2(const unsigned char *buffer, const unsigned int buffer_size, const unsigned int safe_header_only, const file_recovery_t *file_recovery, file_recovery_t *file_recovery_new)
{
  reset_file_recovery(file_recovery_new);
  file_recovery_new->extension=file_hint_tib.extension;
  if(file_recovery_new->blocksize < 512)
    return 1;
  file_recovery_new->file_check=&file_check_tib2;
  file_recovery_new->data_check=&data_check_tib2;
  return 1;
}

static void register_header_check_tib(file_stat_t *file_stat)
{
  static const unsigned char tib_header[4]= { 0xb4, 0x6e, 0x68, 0x44};
  static const unsigned char tib2_header[7]= { 0xce, 0x24, 0xb9, 0xa2, 0x20, 0x00, 0x00};
  register_header_check(0, tib_header,sizeof(tib_header), &header_check_tib, file_stat);
  register_header_check(0, tib2_header,sizeof(tib2_header), &header_check_tib2, file_stat);
}
#endif
