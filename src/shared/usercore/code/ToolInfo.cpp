/*
Copyright (C) 2011 Mark Chandler (Desura Net Pty Ltd)
Copyright (C) 2014 Bad Juju Games, Inc.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software Foundation,
Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA.

Contact us at legal@badjuju.com.

*/

#include "Common.h"
#include "ToolInfo.h"

#include "XMLMacros.h"
#include "sqlite3x.hpp"

#include "managers/WildcardManager.h"

#define X_CONSTANT 0x7FFFFFFF

class OutValI
{
public:
	virtual bool isOperand() = 0;
	virtual ~OutValI(){}
};

namespace
{
	int getOperatorOrder(std::string curOp)
	{
		if (curOp == "<" || curOp == "<=" || curOp == ">" || curOp == ">=")
			return 6;

		if (curOp == "==" || curOp == "!=")
			return 7;

		if (curOp == "&&")
			return 11;

		if (curOp == "||")
			return 12;

		return 20;
	}

	int32 getOpereatorCount(std::string curOp)
	{
		if (curOp == "==" || curOp == "!=" || curOp == ">" || curOp == "<" || curOp == ">=" || curOp == "<=")
			return 2;

		return 0;
	}

	class Operand : public OutValI
	{
	public:
		Operand(int val)
		{
			m_iVal = val;
		}

		virtual bool isOperand()
		{
			return true;
		}

		virtual int getOperand()
		{
			return m_iVal;
		}

	private:
		int32 m_iVal;
	};

	class Operator : public OutValI
	{
	public:
		Operator(std::string val)
			: m_szVal(val)
		{}

		virtual bool isOperand()
		{
			return false;
		}

		virtual std::string& getOperator()
		{
			return m_szVal;
		}

	private:
		std::string m_szVal;
	};

	bool processStack(std::deque<OutValI*> &vOutStack, std::deque<int32> &valStack, std::deque<std::string> &opStack)
	{
		size_t c = getOpereatorCount(opStack.back());

		if (c > valStack.size())
			return false;

		while (c > 0)
		{
			vOutStack.push_back(new Operand(valStack.back()));
			valStack.pop_back();
			c--;
		}

		vOutStack.push_back(new Operator(opStack.back()));
		opStack.pop_back();

		return true;
	}
}

using namespace UserCore;

ToolInfo::ToolInfo(DesuraId id)
:	m_ToolId(id),
	m_uiDownloadSize(0),
	m_uiFlags(0)
{
	m_uiHash = id.toInt64();
}

ToolInfo::~ToolInfo()
{
	safe_delete(m_vRPN);
}

void ToolInfo::parseXml(const XML::gcXMLElement &toolInfoNode, WildcardManager* wildcardManager, const char* appDataPath)
{
	toolInfoNode.GetChild("name", m_szNameString);
	toolInfoNode.GetChild("nameid", m_szName);
	toolInfoNode.GetChild("args", m_szArgs);
	toolInfoNode.GetChild("result", m_szResult);
	toolInfoNode.GetChild("url", m_szUrl);
	toolInfoNode.GetChild("size", m_uiDownloadSize);
	toolInfoNode.GetChild("hash", m_szHash);

	checkFile(appDataPath);

	if (!wildcardManager)
		return;

	char* res = nullptr;
	wildcardManager->constructPath(m_szArgs.c_str(), &res, false);

	m_szArgs = res;
	safe_delete(res);

	auto iChecksNode = toolInfoNode.FirstChildElement("intallchecks");

	if (!iChecksNode.IsValid())
		iChecksNode = toolInfoNode.FirstChildElement("installchecks");

	if (!iChecksNode.IsValid())
		return;

	iChecksNode.for_each_child("installcheck", [&](const XML::gcXMLElement &xmlCheck)
	{
		if (HasAllFlags(m_uiFlags, TF_INSTALLED))
			return;

		const std::string check = xmlCheck.GetText();

		if (check.empty())
			return;

		char* res = nullptr;

		try
		{
			wildcardManager->constructPath(check.c_str(), &res);

			if (res && UTIL::FS::isValidFile(res))
			{
				m_uiFlags |= TF_INSTALLED;

#ifdef NIX
				overridePath(res);
#endif
			}
		}
		catch (gcException)
		{
		}

		safe_delete(res);
	});
}

void ToolInfo::checkFile(const char* appDataPath)
{
#ifdef WIN32
	UTIL::FS::Path path = getPathFromUrl(appDataPath);
	setExePath(path.getFullPath().c_str());
#endif
}

void ToolInfo::saveToDb(sqlite3x::sqlite3_connection* db)
{
	sqlite3x::sqlite3_command cmd(*db, "REPLACE INTO toolinfo VALUES (?,?,?, ?,?,?, ?,?,?, ?);");

	cmd.bind(1, (long long int)m_ToolId.toInt64());
	cmd.bind(2, m_szNameString);
	cmd.bind(3, m_szName);
	cmd.bind(4, m_szUrl);
	cmd.bind(5, UTIL::OS::getRelativePath(m_szExe));
	cmd.bind(6, m_szArgs);
	cmd.bind(7, (int)m_uiFlags);
	cmd.bind(8, (int)m_uiDownloadSize);
	cmd.bind(9, m_szResult);
	cmd.bind(10, m_szHash);

	cmd.executenonquery();
}

void ToolInfo::loadFromDb(sqlite3x::sqlite3_connection* db)
{
	if (!db)
		return;

	try
	{
		sqlite3x::sqlite3_command cmd(*db, "SELECT * FROM toolinfo WHERE internalid=?;");
		cmd.bind(1, (long long int)m_ToolId.toInt64());

		sqlite3x::sqlite3_reader reader = cmd.executereader();

		reader.read();

		//DesuraId id(reader.getint64(0));
		m_szNameString = reader.getstring(1);
		m_szName	= reader.getstring(2);
		m_szUrl		= reader.getstring(3);
		std::string path = UTIL::OS::getAbsPath(reader.getstring(4));
		m_szArgs	= reader.getstring(5);
		m_uiFlags	= reader.getint(6);
		m_uiDownloadSize = reader.getint(7);
		m_szResult	= reader.getstring(8);
		m_szHash	= reader.getstring(9);

		setExePath(path.c_str());
	}
	catch (std::exception &e)
	{
		Warning("Failed to load ToolInfo from db: {0}\n", e.what());
	}
}


DesuraId ToolInfo::getId()
{
	return m_ToolId;
}

const char* ToolInfo::getExe()
{
	return m_szExe.c_str();
}

const char* ToolInfo::getUrl()
{
	return m_szUrl.c_str();
}

const char* ToolInfo::getArgs()
{
	return m_szArgs.c_str();
}

const char* ToolInfo::getName()
{
	return m_szNameString.c_str();
}

const char* ToolInfo::getNameId()
{
	return m_szName.c_str();
}

uint32 ToolInfo::getDownloadSize()
{
	return m_uiDownloadSize;
}

bool ToolInfo::isInstalled()
{
#ifdef NIX
	if (HasAnyFlags(m_uiFlags, TF_LINK))
		return UTIL::FS::isValidFile(m_szExe.c_str());
#endif

	return HasAnyFlags(m_uiFlags, TF_INSTALLED);
}

bool ToolInfo::isDownloaded()
{
	if (!checkExePath(m_szExe.c_str(), true))
		return false;

	return HasAnyFlags(m_uiFlags, TF_DOWNLOADED);
}

#ifdef NIX
void ToolInfo::overridePath(const char* exe)
{
	m_szHash = UTIL::MISC::hashFile(exe);
	m_uiDownloadSize = UTIL::FS::getFileSize(exe);
	m_szExe = exe;
	m_uiFlags = TF_INSTALLED|TF_LINK;
}
#endif

void ToolInfo::setExePath(const char* exe)
{
	if (m_szExe == exe)
		return;

	if (checkExePath(exe))
	{
		m_uiFlags |= TF_DOWNLOADED;
		m_szExe = exe;
	}
	else
	{
		m_uiFlags &= ~TF_DOWNLOADED;
		m_szExe = "";
	}
}

void ToolInfo::setInstalled(bool state)
{
	if (state)
		m_uiFlags |= TF_INSTALLED;
	else
		m_uiFlags &= ~TF_INSTALLED;
}

bool ToolInfo::checkExePath(const char* path, bool quick)
{
	if (!path || !UTIL::FS::isValidFile(path))
		return false;

#ifdef NIX
	if (HasAnyFlags(m_uiFlags, TF_LINK))
		return true;
#endif

	uint64 size = UTIL::FS::getFileSize(path);

	if (size != m_uiDownloadSize)
	{
		UTIL::FS::delFile(path);
		return false;
	}

	if (quick)
		return true;

	gcString hash = UTIL::MISC::hashFile(path);

	bool res = (hash == m_szHash);

	if (!res)
		UTIL::FS::delFile(path);

	return res;
}


UTIL::FS::Path ToolInfo::getPathFromUrl(const char* appDataPath)
{
	UTIL::FS::Path path(appDataPath, "", false);

	path += "tools";
	path += gcString("{0}", getId().getItem());
	path += UTIL::FS::Path(getUrl(), "", true).getFile();

	return path;
}


bool ToolInfo::processResultString()
{
	if (m_szResult.size() == 0)
		return false;

	std::string::iterator it = m_szResult.begin();

	std::deque<int32> valStack;
	std::deque<std::string> opStack;

	bool lastWasDigit = false;
	bool lastWasNeg = false;

	while (it != m_szResult.end())
	{
		char c = *it;

		if (c == ' ')
		{
		}
		else if (c == '(')
		{
			opStack.push_back("(");
		}
		else if (c == ')')
		{
			while (!opStack.empty() && opStack.back() != "(")
			{
				if (!processStack(m_vRPN, valStack, opStack))
					return false;
			}

			if (opStack.empty())
				return false;

			opStack.pop_back();
		}
		else if (c == '-')
		{
			lastWasNeg = true;
		}
		else if (isdigit(c))
		{
			int val = c - 48;

			if (lastWasDigit && !lastWasNeg)
			{
				if (valStack.back() > 0)
					val = valStack.back() * 10 + val;
				else
					val = valStack.back() * 10 - val;

				valStack.pop_back();
			}
			else if (lastWasNeg)
			{
				val = val*-1;
				lastWasNeg = false;
			}

			valStack.push_back(val);
		}
		else if (isalpha(c))
		{
			valStack.push_back(X_CONSTANT);
		}
		else
		{
			std::string val;

			do
			{
				if (c != ' ')
					val.push_back(c);

				++it;
				c = *it;
			}
			while (it != m_szResult.end() && !isdigit(c) && !isalpha(c) && c != '(' && c != ')' && c != '-');

			--it;
			c = *it;


			while (!opStack.empty() && opStack.back() != "(" && (getOperatorOrder(val) > getOperatorOrder(opStack.back())))
			{
				if (!processStack(m_vRPN, valStack, opStack))
					return false;
			}

			opStack.push_back(val);
		}

		lastWasDigit = isdigit(c) || c == '-';

		if (!lastWasDigit)
			lastWasNeg = false;

		++it;
	}


	while (!opStack.empty())
	{
		if (opStack.back() == "(")
			return false;

		if (!processStack(m_vRPN, valStack, opStack))
			return false;
	}

	while (!valStack.empty())
	{
		m_vRPN.push_back(new Operand(valStack.back()));
		valStack.pop_back();
	}

	return true;
}

bool ToolInfo::checkExpectedResult(uint32 res)
{
	if (m_szResult.empty())
		return true;

	if (m_vRPN.empty())
	{
		if (!processResultString())
		{
			Warning("Failed to process result string {0} for {1}, invalid expression.", m_szResult, getName());
			safe_delete(m_vRPN);
			return true;
		}
	}

	if (m_vRPN.size() == 1 && m_vRPN[0]->isOperand())
		return res == (uint32)dynamic_cast<Operand*>(m_vRPN[0])->getOperand();

	std::deque<OutValI*> vList = m_vRPN;
	std::vector<int32> stack;

	while (!vList.empty())
	{
		OutValI* item = vList.front();
		vList.pop_front();

		if (item->isOperand())
		{
			stack.push_back(dynamic_cast<Operand*>(item)->getOperand());
			continue;
		}
		else
		{
			std::string op = dynamic_cast<Operator*>(item)->getOperator();

			if (stack.size() < 2)
			{
				Warning("Bad RPN equation {0} for tool {1}.", m_szResult, getName());
				return true;
			}

			int32 a = stack.back();
			stack.pop_back();

			int32 b = stack.back();
			stack.pop_back();

			if (a == X_CONSTANT)
				a = res;

			if (b == X_CONSTANT)
				b = res;

			int result = 0;

			if (op == "||")
			{
				result = ( a || b );
			}
			else if (op == "&&")
			{
				result = ( a && b );
			}
			else if (op == "!=")
			{
				result = ( a != b );
			}
			else if (op == "==")
			{
				result = ( a == b );
			}
			else if (op == ">")
			{
				result = ( a > b );
			}
			else if (op == ">=")
			{
				result = ( a >= b );
			}
			else if (op == "<")
			{
				result = ( a < b );
			}
			else if (op == "<=")
			{
				result = ( a <= b );
			}
			else
			{
				Warning("Invalid operator {0} in tool result for {1}.", op, getName());
				continue;
			}

			stack.push_back(result);
		}
	}

	if (stack.size() > 1)
		Warning("To many items left on stack after results calc for tool {0}.", getName());

	if (stack.empty())
		return true;

	return stack.front()?true:false;
}

const char* ToolInfo::getResultString()
{
	return m_szResult.c_str();
}

#ifdef WITH_GTEST

namespace UnitTest
{
	class ToolInfoFixture : public ::testing::TestWithParam<int>
	{
	public:
		ToolInfoFixture()
			: m_ToolInfo(DesuraId())
		{

		}

		void SetupResult(const gcString &szRes)
		{
			m_ToolInfo.m_szResult = szRes;
		}

		UserCore::ToolInfo m_ToolInfo;
	};

	INSTANTIATE_TEST_CASE_P(ReturnValues,
		ToolInfoFixture,
		::testing::Values(-1000, -1, 0, 1, 1000));

	TEST_P(ToolInfoFixture, ResultTest_Empty)
	{
		SetupResult("");
		ASSERT_TRUE(m_ToolInfo.checkExpectedResult(GetParam()));
	}

	TEST_P(ToolInfoFixture, ResultTest_Exact_Zero)
	{
		SetupResult("0");

		if (GetParam() == 0)
			ASSERT_TRUE(m_ToolInfo.checkExpectedResult(GetParam()));
		else
			ASSERT_FALSE(m_ToolInfo.checkExpectedResult(GetParam()));
	}

	TEST_P(ToolInfoFixture, ResultTest_EqualZero)
	{
		SetupResult("X==0");

		if (GetParam() == 0)
			ASSERT_TRUE(m_ToolInfo.checkExpectedResult(GetParam()));
		else
			ASSERT_FALSE(m_ToolInfo.checkExpectedResult(GetParam()));
	}

	TEST_P(ToolInfoFixture, ResultTest_NotEqualZero)
	{
		SetupResult("X!=0");

		if (GetParam() != 0)
			ASSERT_TRUE(m_ToolInfo.checkExpectedResult(GetParam()));
		else
			ASSERT_FALSE(m_ToolInfo.checkExpectedResult(GetParam()));
	}

	TEST_P(ToolInfoFixture, ResultTest_Exact_NonZero)
	{
		SetupResult("1000");

		if (GetParam() == 1000)
			ASSERT_TRUE(m_ToolInfo.checkExpectedResult(GetParam()));
		else
			ASSERT_FALSE(m_ToolInfo.checkExpectedResult(GetParam()));
	}

	TEST_P(ToolInfoFixture, ResultTest_Exact_ZeroOr1)
	{
		int nParam = GetParam();

		SetupResult("X == 0 || X == 1");

		if (nParam == 0 || nParam == 1)
			ASSERT_TRUE(m_ToolInfo.checkExpectedResult(nParam));
		else
			ASSERT_FALSE(m_ToolInfo.checkExpectedResult(nParam));
	}

	TEST_P(ToolInfoFixture, ResultTest_GreaterThanZero)
	{
		int nParam = GetParam();

		SetupResult("X > 0");

		if (nParam > 0)
			ASSERT_TRUE(m_ToolInfo.checkExpectedResult(nParam));
		else
			ASSERT_FALSE(m_ToolInfo.checkExpectedResult(nParam));
	}

	TEST_P(ToolInfoFixture, ResultTest_GreaterThanOrEqualZero)
	{
		int nParam = GetParam();

		SetupResult("X >= 0");

		if (nParam >= 0)
			ASSERT_TRUE(m_ToolInfo.checkExpectedResult(nParam));
		else
			ASSERT_FALSE(m_ToolInfo.checkExpectedResult(nParam));
	}

	TEST_P(ToolInfoFixture, ResultTest_LessThanZero)
	{
		int nParam = GetParam();

		SetupResult("X < 0");

		if (nParam < 0)
			ASSERT_TRUE(m_ToolInfo.checkExpectedResult(nParam));
		else
			ASSERT_FALSE(m_ToolInfo.checkExpectedResult(nParam));
	}

	TEST_P(ToolInfoFixture, ResultTest_LessThanOrEqualZero)
	{
		int nParam = GetParam();

		SetupResult("X <= 0");

		if (nParam <= 0)
			ASSERT_TRUE(m_ToolInfo.checkExpectedResult(nParam));
		else
			ASSERT_FALSE(m_ToolInfo.checkExpectedResult(nParam));
	}

	TEST_P(ToolInfoFixture, ResultTest_LessThanOrGreaterThanZero)
	{
		int nParam = GetParam();

		SetupResult("X < 0 || X > 0");

		if (nParam < 0 || nParam > 0)
			ASSERT_TRUE(m_ToolInfo.checkExpectedResult(nParam));
		else
			ASSERT_FALSE(m_ToolInfo.checkExpectedResult(nParam));
	}

	TEST_P(ToolInfoFixture, ResultTest_LessThan1AndGreaterThanNeg1)
	{
		int nParam = GetParam();

		SetupResult("(X > -1) && (X < 1)");

		if ((nParam > -1) && (nParam < 1))
			ASSERT_TRUE(m_ToolInfo.checkExpectedResult(nParam));
		else
			ASSERT_FALSE(m_ToolInfo.checkExpectedResult(nParam));
	}
}

#endif
