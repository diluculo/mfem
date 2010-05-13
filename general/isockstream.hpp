// Copyright (c) 2010,  Lawrence Livermore National Security, LLC.
// Produced at the Lawrence Livermore National Laboratory.
// This file is part of the MFEM library.  See file COPYRIGHT for details.
//
// MFEM is free software; you can redistribute it and/or modify it under the
// terms of the GNU Lesser General Public License (as published by the Free
// Software Foundation) version 2.1 dated February 1999.

#ifndef MFEM_ISOCKSTREAM
#define MFEM_ISOCKSTREAM

#include <signal.h>
#include <sstream>
using namespace std;

/** Data type for input socket stream class. The class is used as server
    to receive data from a client on specified port number. The user gets
    data from the stream as from any other input stream. */
class isockstream
{
private:
   int portnum, portID, socketID, error;
   char *Buf;

   int establish();
   int read_data(int socketid, char *buf, int size);

public:

   /** The constructor takes as input the portnumber port on which
       it establishes a server. */
   isockstream(int port);

   bool good() { return (!error); }

   /// Start waiting for data and return it in an input stream.
   void receive(istringstream **in);

   /** Virtual destructor. If the data hasn't been sent it sends it. */
   ~isockstream();
};

#endif

