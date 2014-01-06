/*
Desura is the leading indie game distribution platform
Copyright (C) 2013 Mark Chandler (Desura Net Pty Ltd)

$LicenseInfo:firstyear=2014&license=lgpl$
Copyright (C) 2014, Linden Research, Inc.

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation;
version 2.1 of the License only.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, see <http://www.gnu.org/licenses/>
or write to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
$/LicenseInfo$
*/

#include "Common.h"
#include "UtilFunction.h"



class CrcCheckMCF : public UtilFunction
{
public:
	virtual uint32 getNumArgs()
	{
		return 1;
	}

	virtual const char* getArgDesc(size_t index)
	{
		return "Src Mcf";
	}

	virtual const char* getFullArg()
	{
		return "crccheck";
	}

	virtual const char getShortArg()
	{
		return 'r';
	}

	virtual const char* getDescription()
	{
		return "Checks to see if all the crc's in the mcf are correct";
	}

	virtual int performAction(std::vector<std::string> &args)
	{
		int res = 0;

		MCFCore::MCFI* mcfSrc = mcfFactory();

		printf("Parsing MCF: %s\n", args[0].c_str());
		mcfSrc->setFile(args[0].c_str());
		mcfSrc->parseMCF();

		bool r = mcfSrc->crcCheck();

		if (r)
		{
			printf("Passed crc check!\n");
			res = 0;
		}
		else
		{
			printf("Failed crc check!\n");
			res = -1;
		}

		mcfDelFactory(mcfSrc);

		return res;
	}
};


class ValidateMCF : public UtilFunction
{
public:
	virtual uint32 getNumArgs()
	{
		return 2;
	}

	virtual const char* getArgDesc(size_t index)
	{
		if (index == 0)
			return "Src Mcf";

		return "Temp Folder";
	}

	virtual const char* getFullArg()
	{
		return "validate";
	}

	virtual const char getShortArg()
	{
		return 'z';
	}

	virtual const char* getDescription()
	{
		return "Validate the mcf to make sure the contents are correct";
	}

	virtual int performAction(std::vector<std::string> &args)
	{
		MCFCore::MCFI* mcfSrc = mcfFactory();

		printf("Parsing MCF: %s\n", args[0].c_str());
		mcfSrc->setFile(args[0].c_str());
		mcfSrc->parseMCF();

		uint32 res = mcfSrc->verifyAll(args[1].c_str());

		mcfDelFactory(mcfSrc);

		return res;
	}
};


class FixMD5 : public UtilFunction
{
public:
	virtual uint32 getNumArgs()
	{
		return 1;
	}

	virtual const char* getArgDesc(size_t index)
	{
		return "Src Mcf";
	}

	virtual const char* getFullArg()
	{
		return "fixmd5";
	}

	virtual const char getShortArg()
	{
		return 'f';
	}

	virtual const char* getDescription()
	{
		return "Fixes compressed md5 checksums and lack of crc's in the mcf. Saves over existing mcf!";
	}

	virtual int performAction(std::vector<std::string> &args)
	{
		MCFCore::MCFI* mcfSrc = mcfFactory();

		printf("Parsing MCF: %s\n", args[0].c_str());
		mcfSrc->setFile(args[0].c_str());
		mcfSrc->parseMCF();

		printf("Fixing md5...\n");
		int res = mcfSrc->fixMD5AndCRC()?1:0;

		mcfDelFactory(mcfSrc);
		return res;
	}
};


REG_FUNCTION(CrcCheckMCF)
REG_FUNCTION(ValidateMCF)
REG_FUNCTION(FixMD5)
