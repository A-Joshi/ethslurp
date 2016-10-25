/*--------------------------------------------------------------------------------
The MIT License (MIT)

Copyright (c) 2016 Great Hill Corporation

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
--------------------------------------------------------------------------------*/
#include "basetypes.h"

#include "runtimeclass.h"

//-------------------------------------------------------------------------
ghRuntimeClass::~ghRuntimeClass(void)
{
	ClearFieldList();
	ASSERT(!m_FieldList);

	if (m_Schema==0xFFFA)
	{
		// This will only happen when the PROTO is read from
		// a file which is not yet supported.
		if (m_ClassName)
			free(m_ClassName);
	}
}

//-------------------------------------------------------------------------
char *ghRuntimeClass::getClassNamePtr(void) const
{
	return m_ClassName;
}

//-------------------------------------------------------------------------
SFBool ghRuntimeClass::IsDerivedFrom(const ghRuntimeClass* pBaseClass) const
{
	const ghRuntimeClass* pClassThis = this;
	while (pClassThis != NULL)
	{
		if (pClassThis == pBaseClass)
			return TRUE;
		pClassThis = pClassThis->m_BaseClass;
	}
	return FALSE;
}

//-------------------------------------------------------------------------
void ghRuntimeClass::Initialize(const SFString& protoName)
{
	SFString copy = protoName;
	if (!copy.IsEmpty())
		m_ClassName = strdup(copy.GetBuffer());

	m_ObjectSize    = 0;
	// Signifies that we were created with Initialize so we can cleanup (i.e. the class is not builtin)
	m_Schema        = 0xFFFA;
	m_BaseClass     = NULL;
	m_FieldList     = NULL;
	m_RefCount      = 0;
	m_CreateFunc    = NULL;
}

//-------------------------------------------------------------------------
void ghRuntimeClass::ClearFieldList(void)
{
	if (m_FieldList)
	{
		LISTPOS p = m_FieldList->GetFirstItem();
		while (p)
		{
			CFieldData *field = m_FieldList->GetNextItem(p);
			delete field;
		}
		m_FieldList->RemoveAll();
		delete m_FieldList;
		m_FieldList = NULL;
	}
}

//-------------------------------------------------------------------------
SFString ghRuntimeClass::listOfFields(char sep) const
{
	SFString ret;
	if (m_FieldList)
	{
		LISTPOS p = m_FieldList->GetFirstItem();
		while (p)
		{
			CFieldData *field = m_FieldList->GetNextItem(p);
			ret += field->getFieldName() + sep;
		}
	}
	return ret;
}

//-------------------------------------------------------------------------
void ghRuntimeClass::AddField(const SFString& fieldName, SFInt32 dataType, SFInt32 fieldID)
{
	if (!m_FieldList)
		m_FieldList = new CFieldList;
	ASSERT(m_FieldList);

	CFieldData *field = new CFieldData("", fieldName, fieldName, "", dataType, FALSE, fieldID);
	m_FieldList->AddTail(field);
}

//-------------------------------------------------------------------------
SFInt32 ghRuntimeClass::Reference(void)
{
	++m_RefCount;
	return m_RefCount;
}

//-------------------------------------------------------------------------
SFInt32 ghRuntimeClass::Dereference(void)
{
	m_RefCount--;
	ASSERT(m_RefCount>=0);

	if (!m_RefCount || (m_CreateFunc && m_RefCount==1))
	{
		// Clear the field list for proper PROTO's when last
		// referenced or BuiltIns when only the builtin is still referenced.
		ClearFieldList();
	}

	return (m_RefCount==0);
}

//-------------------------------------------------------------------------
CBuiltIn::CBuiltIn(ghRuntimeClass *pClass, const SFString& className, SFInt32 size, PFNV createFunc, ghRuntimeClass *pBase, SFInt32 schema)
{
	m_pClass = pClass;
	SFString copy = className;
	if (!copy.IsEmpty())
		pClass->m_ClassName = strdup(copy.GetBuffer());

	pClass->m_ObjectSize    = size;
	// Signifies that we were created with Initialize so we can cleanup (i.e. the class is not builtin)
	pClass->m_Schema        = schema;
	pClass->m_BaseClass     = pBase;
	pClass->m_FieldList     = NULL;
	pClass->m_RefCount      = 0;
	pClass->m_CreateFunc    = createFunc;
}

#include "basenode.h"
//--------------------------------------------------------------------------------
SFString nextBasenodeChunk(const SFString& fieldIn, SFBool force, const CBaseNode *node)
{
	SFString className = node->getRuntimeClass()->getClassNamePtr();
	switch (tolower(fieldIn[0]))
	{
		case 'd':
			if ( fieldIn % "deleted" ) return asString(node->isDeleted());;
			break;
		case 'n':
			if ( fieldIn % "null" ) return "<x>";
			break;
		case 's':
			if ( fieldIn % "schema" ) return asString(node->getSchema());
			if ( fieldIn % "showing" ) return asString(node->isShowing());
			break;
		default:
			break;
	}

	return EMPTY;
}
