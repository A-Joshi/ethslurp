#ifndef _VOTE_H_
#define _VOTE_H_
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
/*
 * This file was generated with makeClass. Edit only those parts of the code inside
 * of 'EXISTING_CODE' tags.
 */
#include "utillib.h"
#include "etherlib.h"
#include "transaction.h"

//--------------------------------------------------------------------------
class CVote;
typedef SFArrayBase<CVote>         CVoteArray;
typedef SFList<CVote*>             CVoteList;
typedef CNotifyClass<const CVote*> CVoteNotify;
typedef SFUniqueList<CVote*>       CVoteListU;

//---------------------------------------------------------------------------
extern int sortVote        (const SFString& f1, const SFString& f2, const void *rr1, const void *rr2);
extern int sortVoteByName  (const void *rr1, const void *rr2);
extern int sortVoteByID    (const void *rr1, const void *rr2);
extern int isDuplicateVote (const void *rr1, const void *rr2);

// EXISTING_CODE
// EXISTING_CODE

//--------------------------------------------------------------------------
class CVote : public CTransaction
{
public:
	SFAddress voter;
	SFInt32 proposalID;
	SFBool votedYes;

public:
					CVote  (void);
					CVote  (const CVote& vo);
				   ~CVote  (void);
	CVote&	operator= 		(const CVote& vo);

	DECLARE_NODE (CVote);

	// EXISTING_CODE
	// EXISTING_CODE

protected:
	void			Clear      		(void);
	void			Init      		(void);
	void			Copy      		(const CVote& vo);
	SFBool                  readBackLevel           (SFArchive& archive);

	// EXISTING_CODE
	// EXISTING_CODE
};

//--------------------------------------------------------------------------
inline CVote::CVote(void)
{
	Init();
	// EXISTING_CODE
	// EXISTING_CODE
}

//--------------------------------------------------------------------------
inline CVote::CVote(const CVote& vo)
{
	// EXISTING_CODE
	// EXISTING_CODE
	Copy(vo);
}

// EXISTING_CODE
// EXISTING_CODE

//--------------------------------------------------------------------------
inline CVote::~CVote(void)
{
	Clear();
	// EXISTING_CODE
	// EXISTING_CODE
}

//--------------------------------------------------------------------------
inline void CVote::Clear(void)
{
	// EXISTING_CODE
	// EXISTING_CODE
}

//--------------------------------------------------------------------------
inline void CVote::Init(void)
{
	CTransaction::Init();

	voter = EMPTY;
	proposalID = 0;
	votedYes = 0;

	// EXISTING_CODE
	// EXISTING_CODE
}

//--------------------------------------------------------------------------
inline void CVote::Copy(const CVote& vo)
{
	Clear();

	CTransaction::Copy(vo);
	voter = vo.voter;
	proposalID = vo.proposalID;
	votedYes = vo.votedYes;

	// EXISTING_CODE
	// EXISTING_CODE
	finishParse();
}

//--------------------------------------------------------------------------
inline CVote& CVote::operator=(const CVote& vo)
{
	Copy(vo);
	// EXISTING_CODE
	// EXISTING_CODE
	return *this;
}

//---------------------------------------------------------------------------
inline SFString CVote::getValueByName(const SFString& fieldName) const
{
	// EXISTING_CODE
	// EXISTING_CODE
	return Format("[{"+toUpper(fieldName)+"}]");
}

//---------------------------------------------------------------------------
extern SFString nextVoteChunk(const SFString& fieldIn, SFBool& force, const void *data);

//---------------------------------------------------------------------------
IMPLEMENT_ARCHIVE_ARRAY(CVoteArray);
IMPLEMENT_ARCHIVE_LIST(CVoteList);

//---------------------------------------------------------------------------
extern SFString nextVoteChunk_custom(const SFString& fieldIn, SFBool& force, const void *data);

//---------------------------------------------------------------------------
// EXISTING_CODE
// EXISTING_CODE

#endif
