/*
  Zipios++ - a small C++ library that provides easy access to .zip files.
  Copyright (C) 2000-2015  Thomas Sondergaard

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307  USA
*/

/** \file
 * \brief Implementation of zip header handling.
 *
 * Implementation of routines for reading the central directory and
 * local headers of a zip archive.
 */

#include "zipcdirentry.hpp"

#include "zipios++/zipiosexceptions.hpp"

#include "dostime.h"
#include "zipios_common.hpp"


namespace zipios
{



namespace
{


// "PK 1.2"
uint32_t const  g_signature = 0x02014b50;

// The zip codes
uint16_t const   g_msdos         = 0x0000;
uint16_t const   g_amiga         = 0x0100;
uint16_t const   g_open_vms      = 0x0200;
uint16_t const   g_unix          = 0x0300;
uint16_t const   g_vm_cms        = 0x0400;
uint16_t const   g_atari_st      = 0x0500;
uint16_t const   g_os2_hpfs      = 0x0600;
uint16_t const   g_macintosh     = 0x0700;
uint16_t const   g_z_system      = 0x0800;
uint16_t const   g_cpm           = 0x0900;
uint16_t const   g_windows       = 0x0A00;
uint16_t const   g_mvs           = 0x0B00;
uint16_t const   g_vse           = 0x0C00;
uint16_t const   g_acorn_risc    = 0x0D00;
uint16_t const   g_vfat          = 0x0E00;
uint16_t const   g_alternate_vms = 0x0F00;
uint16_t const   g_beos          = 0x1000;
uint16_t const   g_tandem        = 0x1100;
uint16_t const   g_os400         = 0x1200;
uint16_t const   g_osx           = 0x1300;



} // no name namespace


/** \class ZipCDirEntry
 * \brief A specialization of ZipLocalEntry for
 *
 * Specialization of ZipLocalEntry, that add fields for storing the
 * extra information, that is only present in the entries in the zip
 * central directory and not in the local entry headers.
 */


ZipCDirEntry::ZipCDirEntry(std::string const& filename, std::string const& file_comment, buffer_t const& extra_field)
    : ZipLocalEntry(filename, extra_field)
    // TODO -- missing initialization of many member variables
    //, m_disk_num_start(0) -- auto-init
    //, m_intern_file_attr(0) -- auto-init
    //, m_extern_file_attr(0x81B40000) -- auto-init

    // FIXME: I do not understand the external mapping, simply
    //        copied value for a file with -rw-rw-r-- permissions
    //        compressed with info-zip
{
    setComment(file_comment);
    setDefaultWriter();
}


ZipCDirEntry& ZipCDirEntry::operator = (ZipCDirEntry const& src)
{
    m_filename            = src.m_filename            ;
    m_uncompressed_size   = src.m_uncompressed_size   ;
    m_unix_time           = src.m_unix_time           ;
    m_crc_32              = src.m_crc_32              ;

    m_writer_version      = src.m_writer_version      ;
    m_extract_version     = src.m_extract_version     ;
    m_gp_bitfield         = src.m_gp_bitfield         ;
    m_compress_method     = src.m_compress_method     ;
    m_compressed_size     = src.m_compressed_size     ;
    m_disk_num_start      = src.m_disk_num_start      ;
    m_intern_file_attr    = src.m_intern_file_attr    ;
    m_extern_file_attr    = src.m_extern_file_attr    ;
    m_rel_offset_loc_head = src.m_rel_offset_loc_head ;
    m_extra_field         = src.m_extra_field         ;
    m_file_comment        = src.m_file_comment        ;

    return *this;
}





void ZipCDirEntry::setDefaultWriter()
{
    // reset version
    m_writer_version = g_zip_format_version;

    // then add "compatibility" code
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32)
    // MS-Windows (TODO: should we use g_msdos instead?)
    m_writer_version |= g_windows;
#elif defined(__APPLE__) && defined(__MACH__)
    // OS/X
    m_writer_version |= g_osx;
#else
    // Other Unices
    m_writer_version |= g_unix;
#endif
}


std::string ZipCDirEntry::getComment() const
{
    return m_file_comment;
}


offset_t ZipCDirEntry::getLocalHeaderOffset() const
{
    return m_rel_offset_loc_head;
}

void ZipCDirEntry::setLocalHeaderOffset(offset_t offset)
{
    m_rel_offset_loc_head = offset;
}


void ZipCDirEntry::setComment(std::string const& comment)
{
    m_file_comment = comment;
}


std::string ZipCDirEntry::toString() const
{
    OutputStringStream sout;
    sout << m_filename << " (" << m_uncompressed_size << " bytes, ";
    sout << m_compressed_size << " bytes compressed)";
    return sout.str();
}


int ZipCDirEntry::getCDirHeaderSize() const
{
    return 46 + m_filename.size() + m_extra_field.size() + m_file_comment.size() ;
}


FileEntry::pointer_t ZipCDirEntry::clone() const
{
    return FileEntry::pointer_t(new ZipCDirEntry(*this));
}



void ZipCDirEntry::read(std::istream& is)
{
    m_valid = false; // set to true upon successful completion.
    if(!is)
    {
        return;
    }

    uint32_t signature;
    zipRead(is, signature);

    if(g_signature != signature)
    {
        // put stream in error state and return
        is.setstate(std::ios::failbit);
        return;
    }

    uint16_t compress_method(0);
    uint32_t dostime(0);
    uint32_t compressed_size(0);
    uint32_t uncompressed_size(0);
    uint32_t rel_offset_loc_head(0);
    uint16_t filename_len(0);
    uint16_t extra_field_len(0);
    uint16_t file_comment_len(0);
    std::string filename;

    // read the header
    zipRead(is, m_writer_version);                  // 16
    zipRead(is, m_extract_version);                 // 16
    zipRead(is, m_gp_bitfield);                     // 16
    zipRead(is, compress_method);                   // 16
    zipRead(is, dostime);                           // 32
    zipRead(is, m_crc_32);                          // 32
    zipRead(is, compressed_size);                   // 32
    zipRead(is, uncompressed_size);                 // 32
    zipRead(is, filename_len);                      // 16
    zipRead(is, extra_field_len);                   // 16
    zipRead(is, file_comment_len);                  // 16
    zipRead(is, m_disk_num_start);                  // 16
    zipRead(is, m_intern_file_attr);                // 16
    zipRead(is, m_extern_file_attr);                // 32
    zipRead(is, rel_offset_loc_head);               // 32
    zipRead(is, filename, filename_len);            // string
    zipRead(is, m_extra_field, extra_field_len);    // buffer
    zipRead(is, m_file_comment, file_comment_len);  // string
    // TODO: check whether this was a 64 bit header and make sure
    //       to read the 64 bit header  too

    m_compress_method = static_cast<StorageMethod>(compress_method);
    m_unix_time = dos2unixtime(dostime);
    m_compressed_size = compressed_size;
    m_uncompressed_size = uncompressed_size;
    m_rel_offset_loc_head = rel_offset_loc_head;
    m_filename = FilePath(filename);

    // the zipRead() should throw if is is false...
    if(is)
    {
        m_valid = true;
    }
}


void ZipCDirEntry::write(std::ostream& os)
{
    if(os)
    {
        // TODO: add support for 64 bit entries
        //       (zip64 is available, just need to add a 64 bit header)
        if(m_compressed_size >= 0x100000000
        || m_uncompressed_size >= 0x100000000)
        {
            throw InvalidStateException("The size of this file is too large to fit in a zip archive.");
        }

        uint16_t compress_method(static_cast<uint8_t>(m_compress_method));
        uint32_t dostime(unix2dostime(m_unix_time));
        uint32_t compressed_size(m_compressed_size);
        uint32_t uncompressed_size(m_uncompressed_size);
        uint16_t filename_len(m_filename.length());
        uint16_t extra_field_len(m_extra_field.size());
        uint16_t file_comment_len(m_file_comment.length());
        uint32_t rel_offset_loc_head(m_rel_offset_loc_head);

        zipWrite(os, g_signature           );       // 32
        zipWrite(os, m_writer_version      );       // 16
        zipWrite(os, m_extract_version     );       // 16
        zipWrite(os, m_gp_bitfield         );       // 16
        zipWrite(os, compress_method       );       // 16
        zipWrite(os, dostime               );       // 32
        zipWrite(os, m_crc_32              );       // 32
        zipWrite(os, compressed_size       );       // 32
        zipWrite(os, uncompressed_size     );       // 32
        zipWrite(os, filename_len          );       // 16
        zipWrite(os, extra_field_len       );       // 16
        zipWrite(os, file_comment_len      );       // 16
        zipWrite(os, m_disk_num_start      );       // 16
        zipWrite(os, m_intern_file_attr    );       // 16
        zipWrite(os, m_extern_file_attr    );       // 32
        zipWrite(os, rel_offset_loc_head   );       // 32
        zipWrite(os, m_filename            );       // string
        zipWrite(os, m_extra_field         );       // string
        zipWrite(os, m_file_comment        );       // string
    }
}


} // zipios namespace
// vim: ts=4 sw=4 et