#ifndef _OPTIONS_H_
#define _OPTIONS_H_
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
class COptions
{
public:
	static SFString msg;
	static SFBool useVerbose;
	static SFBool useTesting;

	SFString commandList;
	SFBool   fromFile;

	               COptions        (void) { fromFile = FALSE; }
	    virtual   ~COptions        (void) { }
		SFBool prepareArguments(int argc, const char *argv[]);
	virtual SFBool parseArguments  (SFString& command) = 0;

protected:
	virtual void Init (void) = 0;
};

//--------------------------------------------------------------------------------
class CParams
{
public:
	SFString  shortName;
	SFString  longName;
	SFString  description;
	CParams( const SFString& name, const SFString& descr );
};

//--------------------------------------------------------------------------------
extern int      usage       (const SFString& errMsg=nullString);
extern SFString options     (void);
extern SFString descriptions(void);
extern SFString purpose     (void);

//--------------------------------------------------------------------------------
extern int      sortParams  (const void *c1, const void *c2);
extern SFString expandOption(SFString& arg);

//--------------------------------------------------------------------------------
extern CParams params[];
extern SFInt32 nParams;

//--------------------------------------------------------------------------------
extern SFBool verbose;
extern SFBool isTesting;

//--------------------------------------------------------------------------------
extern CFileExportContext   outScreen;
extern CFileExportContext&  outErr;

//--------------------------------------------------------------------------------
extern SFString cachePath (const SFString& part=EMPTY);
extern SFString configPath(const SFString& part=EMPTY);

#endif
