///////////// Copyright 2010 Desura Pty Ltd. All rights reserved.  /////////////
//
//   Project     : desura_libcef_dll_wrapper
//   File        : SchemeExtender.cpp
//   Description :
//      [TODO: Write the purpose of SchemeExtender.cpp.]
//
//   Created On: 6/17/2010 4:32:23 PM
//   Created By: Mark Chandler <mailto:mark@moddb.com>
////////////////////////////////////////////////////////////////////////////

#include "SchemeExtender.h"
#include "SchemeRequest.h"
#include "SchemePost.h"
#include "include/wrapper/cef_helpers.h"

#include <map>
#include <algorithm>

class SchemeHandlerFactory;

std::map<std::string, SchemeHandlerFactory* > g_mSchemeExtenders;

class SchemeHandlerFactory : public CefSchemeHandlerFactory
{
public:
	SchemeHandlerFactory()
	{
	}

	~SchemeHandlerFactory()
	{
		if (m_mSchemeMap.size() > 0)
			g_mSchemeExtenders[m_mSchemeMap.begin()->second->getSchemeName()] = 0;

		std::for_each(m_mSchemeMap.begin(), m_mSchemeMap.end(), for_each_del);
		m_mSchemeMap.clear();
	}

	static void for_each_del(std::pair<std::string, ChromiumDLL::SchemeExtenderI*> p)
	{
		p.second->destroy();
	}

	CefRefPtr<CefResourceHandler> Create(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, const CefString& scheme_name, CefRefPtr<CefRequest> request)
	{
		CEF_REQUIRE_IO_THREAD();

		std::string url = request->GetURL();
		std::vector<size_t> slashes;

		for (size_t x=0; x<url.size(); x++)
		{
			if (url[x] == '/')
				slashes.push_back(x);
		}

		if (slashes.size() < 3)
			return nullptr;

		std::string host = url.substr(slashes[1]+1, slashes[2]-slashes[1]-1);
		std::map<std::string, ChromiumDLL::SchemeExtenderI*>::iterator it = m_mSchemeMap.find(host);

		if (it == m_mSchemeMap.end())
			return nullptr;

		CefRefPtr<SchemeExtender> ptr = new SchemeExtender( it->second->clone( (const char*) scheme_name.c_str() ) );
		return ptr;
	}

	bool registerScheme(ChromiumDLL::SchemeExtenderI* se)
	{
		if (m_mSchemeMap[se->getHostName()])
			m_mSchemeMap[se->getHostName()]->destroy();

		m_mSchemeMap[se->getHostName()] = se;

		return CefRegisterSchemeHandlerFactory(se->getSchemeName(), se->getHostName(), (CefSchemeHandlerFactory*) this);
	}

	IMPLEMENT_REFCOUNTING( SchemeHandlerFactory );

private:
	std::map<std::string, ChromiumDLL::SchemeExtenderI*> m_mSchemeMap;
};



bool SchemeExtender::Register(ChromiumDLL::SchemeExtenderI* se)
{
	if (!se)
		return false;

	if (!g_mSchemeExtenders[se->getSchemeName()])
		g_mSchemeExtenders[se->getSchemeName()] = new SchemeHandlerFactory();

	return g_mSchemeExtenders[se->getSchemeName()]->registerScheme(se);
}








SchemeExtender::SchemeExtender(ChromiumDLL::SchemeExtenderI* se)
	: m_NeedRedirect( false )
	, m_pSchemeExtender( se )
{
	if (m_pSchemeExtender)
		se->registerCallback(this);
}

SchemeExtender::~SchemeExtender()
{
	if (m_pSchemeExtender)
		m_pSchemeExtender->destroy();
}


bool SchemeExtender::ProcessRequest(CefRefPtr<CefRequest> request, CefRefPtr<CefCallback> callback)
{
	if (!m_pSchemeExtender)
		return false;

	m_Callback = callback;

	SchemeRequest r(request);

	bool result =  m_pSchemeExtender->processRequest( &r, &m_NeedRedirect );

	if ( result && m_Callback.get() )
		m_Callback->Continue();

	return result;
}

void SchemeExtender::Cancel()
{
	if (!m_pSchemeExtender)
		return;

	m_pSchemeExtender->cancel();
}

void SchemeExtender::GetResponseHeaders( CefRefPtr<CefResponse> response, int64& response_length, CefString& redirectUrl )
{
	if (!m_pSchemeExtender)
		return;

	response_length = m_pSchemeExtender->getResponseSize();
	const char* mime = m_pSchemeExtender->getResponseMimeType();

	if (mime)
		response->SetMimeType(mime);

	if ( m_NeedRedirect )
	{
		m_NeedRedirect = false;
		const char *szRUrl = m_pSchemeExtender->getRedirectUrl();

		if ( szRUrl )
			redirectUrl.FromASCII( szRUrl );
	}
}

bool SchemeExtender::ReadResponse(void* data_out, int bytes_to_read, int& bytes_read, CefRefPtr<CefCallback> callback)
{
	if (!m_pSchemeExtender)
		return false;

	m_Callback = callback;
	return m_pSchemeExtender->read((char*)data_out, bytes_to_read, &bytes_read);
}

void SchemeExtender::responseReady()
{
	if (m_Callback.get())
		m_Callback->Continue();
}

void SchemeExtender::dataReady()
{
	if (m_Callback.get())
		m_Callback->Continue();
}

void SchemeExtender::cancel()
{
	if (m_Callback.get())
		m_Callback->Cancel();
}