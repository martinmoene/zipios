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
 * \brief This file is the implementation of zipios::GZIPOutputStreambuf class.
 *
 * This class is an output stream filter which knows how to creates a .gz
 * file from the data you write to it.
 *
 * The compression makes use of the zlib library.
 */

#include "gzipoutputstreambuf.hpp"


namespace zipios
{


/** \brief Initialize a GZIPOutputStreambuf object.
 *
 * A newly constructed GZIPOutputStreambuf is ready to accept data.
 *
 * \param[in,out] outbuf  The streambuf to use for output.
 */
GZIPOutputStreambuf::GZIPOutputStreambuf(std::streambuf *outbuf)
    : DeflateOutputStreambuf(outbuf, true)
    //, m_open(false) -- auto-init
{
}


/** \brief Ensures that the stream gets closed properly.
 *
 * This function makes sure that the stream gets closed properly
 * which means that the compress terminates by calling finish()
 * and the streams get closed.
 */
GZIPOutputStreambuf::~GZIPOutputStreambuf()
{
    finish();
}


void GZIPOutputStreambuf::setFilename(std::string const& filename)
{
    m_filename = filename;
}


void GZIPOutputStreambuf::setComment(std::string const& comment)
{
    m_comment = comment;
}


/** \brief Close the stream.
 *
 * This function ensures that the streams get closed.
 */
void GZIPOutputStreambuf::close()
{
    finish();
}


/** \brief Finishes the compression.
 *
 * Write whatever is still necessary and close the streams.
 */
void GZIPOutputStreambuf::finish()
{
    if(!m_open)
    {
        return;
    }
    m_open = false;

    closeStream();
    writeTrailer();
}


int GZIPOutputStreambuf::overflow(int c)
{
    if(!m_open)
    {
        writeHeader();
        m_open = true;
    }

    return DeflateOutputStreambuf::overflow(c);
}


int GZIPOutputStreambuf::sync()
{
    return DeflateOutputStreambuf::sync();
}


void GZIPOutputStreambuf::writeHeader()
{
    unsigned char const flg(
                  (m_filename.empty() ? 0x00 : 0x08)
                | (m_comment.empty()  ? 0x00 : 0x10)
            );

    /** \TODO:
     * We need to know of the last modification time instead of
     * saving all zeros for MTIME values.
     *
     * Also, I am thinking that the OS should be 3 under Unices.
     */

    std::ostream os(m_outbuf) ;
    os << static_cast<unsigned char>(0x1f);  // Magic #
    os << static_cast<unsigned char>(0x8b);  // Magic #
    os << static_cast<unsigned char>(0x08);  // Deflater.DEFLATED
    os << flg;                               // FLG
    os << static_cast<unsigned char>(0x00);  // MTIME
    os << static_cast<unsigned char>(0x00);  // MTIME
    os << static_cast<unsigned char>(0x00);  // MTIME
    os << static_cast<unsigned char>(0x00);  // MTIME
    os << static_cast<unsigned char>(0x00);  // XFLG
    os << static_cast<unsigned char>(0x00);  // OS

    if(!m_filename.empty())
    {
        os << m_filename.c_str();             // Filename
        os << static_cast<unsigned char>(0x00);
    }

    if(!m_comment.empty())
    {
        os << m_comment.c_str();              // Comment
        os << static_cast<unsigned char>(0x00);
    }
}


void GZIPOutputStreambuf::writeTrailer()
{
    // write the CRC32 and Size at the end of the file
    writeInt(getCrc32());
    writeInt(getCount());
}


void GZIPOutputStreambuf::writeInt(uint32_t i)
{
    /** \TODO: add support for 64 bit files if it exists? */
    std::ostream os(m_outbuf);
    os << static_cast<unsigned char>( i        & 0xFF);
    os << static_cast<unsigned char>((i >>  8) & 0xFF);
    os << static_cast<unsigned char>((i >> 16) & 0xFF);
    os << static_cast<unsigned char>((i >> 24) & 0xFF);
}


} // zipios namespace
// vim: ts=4 sw=4 et

// Local Variables:
// mode: cpp
// indent-tabs-mode: nil
// c-basic-offset: 4
// tab-width: 4
// End:
