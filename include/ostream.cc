/***************************************************************************
 *
 * ostream.cc - Definitions for the Standard Library ostream classes
 *
 * $Id$
 *
 ***************************************************************************
 *
 * Licensed to the Apache Software  Foundation (ASF) under one or more
 * contributor  license agreements.  See  the NOTICE  file distributed
 * with  this  work  for  additional information  regarding  copyright
 * ownership.   The ASF  licenses this  file to  you under  the Apache
 * License, Version  2.0 (the  "License"); you may  not use  this file
 * except in  compliance with the License.   You may obtain  a copy of
 * the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the  License is distributed on an  "AS IS" BASIS,
 * WITHOUT  WARRANTIES OR CONDITIONS  OF ANY  KIND, either  express or
 * implied.   See  the License  for  the  specific language  governing
 * permissions and limitations under the License.
 *
 * Copyright 1994-2008 Rogue Wave Software, Inc.
 * 
 **************************************************************************/


namespace std { 


template<class _CharT, class _Traits>
basic_ostream<_CharT, _Traits>&
basic_ostream<_CharT, _Traits>::flush ()
{
    if (this->rdbuf ()) {

        // see LWG issue 581

        const sentry __opfx (*this);

        if (__opfx)
            _C_sync_rdbuf ();
    }

    return *this;
}


template<class _CharT, class _Traits>
void
basic_ostream<_CharT, _Traits>::
_C_sync_chain (const basic_streambuf<char_type, traits_type> *__buf)
{
    // count the streams on the chain, each once: the hare, two steps
    // to the tortoise's one, falls off a chain that ends in a null
    // and meets the tortoise on a ring
    const basic_ostream *__p0 = this;
    const basic_ostream *__p1 = this;

    bool __ring = false;

    while (__p1->tie () && __p1->tie ()->tie ()) {
        __p0 = __p0->tie ();
        __p1 = __p1->tie ()->tie ();

        if (__p0 == __p1) {
            __ring = true;
            break;
        }
    }

    _RWSTD_SIZE_T __n = 1;

    if (__ring) {
        // the tortoise restarted at *this and the hare, both one step
        // at a time, meet at the first stream of the ring
        for (__p0 = this; __p0 != __p1; ++__n) {
            __p0 = __p0->tie ();
            __p1 = __p1->tie ();
        }

        // one lap from there is the length of the ring; __n counts
        // the streams before it plus one, and the lap the rest
        for (__p1 = __p0->tie (); __p1 != __p0; ++__n)
            __p1 = __p1->tie ();
    }
    else {
        for (__p0 = this; __p0->tie (); ++__n)
            __p0 = __p0->tie ();
    }

    // the chain ends before the first stream that is not good (),
    // whose own sentry would flush neither its buffer nor its tie
    // (27.6.2.3, p2)
    _RWSTD_SIZE_T __ngood = 0;

    for (__p0 = this; __ngood < __n && __p0->good (); ++__ngood)
        __p0 = __p0->tie ();

    if (__ngood)
        _C_sync_tail (__ngood, __buf);
}


template<class _CharT, class _Traits>
void
basic_ostream<_CharT, _Traits>::
_C_sync_tail (_RWSTD_SIZE_T                                   __n,
              const basic_streambuf<char_type, traits_type> *__buf)
{
    // the farthest stream first, as its sentry is the innermost of
    // the nested flushes of 27.6.2.3, p2
    if (1 < __n)
        this->tie ()->_C_sync_tail (__n - 1, __buf);

    // what flush () does inside its sentry, under the same gate
    if (this->rdbuf () && this->rdbuf () != __buf) {
        _RWSTD_MT_GUARD (this->_C_bufmutex ());

        if (this->good ()) {
            if (_C_is_sync ())
                __rw::__rw_fflush (static_cast< ios_base* >(this), 0);

            _C_sync_rdbuf ();
        }
    }
}


template<class _CharT, class _Traits>
void
basic_ostream<_CharT, _Traits>::_C_sync_rdbuf ()
{
    int __res = 0;

    try {
        __res = this->rdbuf ()->pubsync ();
    }
    catch (...) {
        this->setstate (ios_base::badbit | __rw::__rw_rethrow);
    }

    if (-1 == __res)
        this->setstate (ios_base::badbit);
}
  

template<class _CharT, class _Traits>
basic_ostream<_CharT, _Traits>&
basic_ostream<_CharT, _Traits>::seekp (pos_type __pos)
{
    _RWSTD_ASSERT (0 != this->rdbuf ());

    if (!this->fail ()) {
        _RWSTD_MT_GUARD (this->_C_bufmutex ());

        pos_type __res = 0;

        try {
            __res = this->rdbuf ()->pubseekpos (__pos, ios_base::out);
        }
        catch (...) {
            this->setstate (ios_base::badbit | __rw::__rw_rethrow);
        }
        if (-1 == __res)
            this->setstate (ios_base::failbit);   // lwg issue 129
    }

    return *this;
}


template<class _CharT, class _Traits>
basic_ostream<_CharT, _Traits>&
basic_ostream<_CharT, _Traits>::seekp (off_type __off, ios_base::seekdir __dir)
{
    _RWSTD_ASSERT (0 != this->rdbuf ());

    if (!this->fail ()) {
        _RWSTD_MT_GUARD (this->_C_bufmutex ());

        off_type __res = 0;

        try {
            __res = this->rdbuf()->pubseekoff (__off, __dir, ios_base::out);
        }
        catch (...) {
            this->setstate (ios_base::badbit | __rw::__rw_rethrow);
        }

        if (-1 == __res)
            this->setstate (ios_base::failbit);   // lwg issue 129
    }

    return *this;
}


template<class _CharT, class _Traits>
typename basic_ostream<_CharT, _Traits>::pos_type
basic_ostream<_CharT, _Traits>::tellp ()
{
    _RWSTD_ASSERT (0 != this->rdbuf ());

    if (!this->fail ()) {
        _RWSTD_MT_GUARD (this->_C_bufmutex ());

        try {
            return this->rdbuf()->pubseekoff (0, ios_base::cur, ios_base::out);
        }
        catch (...) {
            this->setstate (ios_base::badbit | __rw::__rw_rethrow);
        }
    }

    return pos_type (off_type (-1));
}


template<class _CharT, class _Traits>
basic_ostream<_CharT, _Traits>&
basic_ostream<_CharT, _Traits>::
operator<< (basic_streambuf<char_type, traits_type> *__sb)
{ 
    if (!__sb) {
        this->setstate (ios_base::badbit);
        return *this;
    }

    ios_base::iostate __err = ios_base::goodbit;

    streamsize __nget = 0;   // number of extracted characters
    
    if (__sb->_C_mode () & ios_base::in) {

        const sentry __opfx (*this);

        try {
            
            if (__opfx) {

                basic_streambuf<char_type, traits_type>* const __rdbuf =
                    this->rdbuf ();

                for (const int_type __eof = traits_type::eof (); ; ++__nget) {

                    // determine the amount of available space in our buffer
                    // into which it is safe (i.e., w/o calling overridden
                    // virtuals on the buffer object or risking exceptions)
                    // to copy the contents of the source stream buffer
                    const streamsize __navail =
                        streamsize (__rdbuf->epptr () - __rdbuf->pptr ());

                    if (__navail > 1) {
                        // copy 2 or more characters directly into buffer
                        const streamsize __n =
                            __sb->sgetn (__rdbuf->pptr (), __navail);

                        if (__n < 0) {
                            __err = ios_base::failbit;
                            break;
                        }

                        if (!__n)
                            break;

                        __rdbuf->pbump (__n);

                        __nget += __n;
                    }
                    else {
                        // if space for less than 2 characters is available
                        // try to retrieve and insert a single character
                        int_type __c = __sb->sgetc ();

                        // 27.6.2.5.3, p7, bullet 1
                        if (traits_type::eq_int_type (__eof, __c))
                            break;

                        __c = __rdbuf->sputc (traits_type::to_char_type(__c));

                        if (traits_type::eq_int_type (__eof, __c)) {
                            __err = ios_base::failbit;
                            break;
                        }

                        // 27.6.2.5.3, p7, bullet 2: extract character
                        // only if it has been successfully inserted
                        __sb->snextc ();
                    }
                }
            }
        }
        catch (...) {
            this->setstate (ios_base::badbit | __rw::__rw_rethrow);
        }
    }
    // 27.6.2.5.3.8
    if (!__nget) 
        __err |= ios_base::failbit;
    
    if (__err)
        this->setstate (__err);

    return *this;
}


}   // namespace std
