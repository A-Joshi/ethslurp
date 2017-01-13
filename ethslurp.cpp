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
#include "ethslurp.h"
#include "slurp_options.h"

extern int sortReverseChron(const void *rr1, const void *rr2);
//--------------------------------------------------------------------------------
int main(int argc, const char * argv[])
{
	// We keep only a single slurper. If the user is using the --file option and they
	// are reading the same account repeatedly, we only need to read the cache once.
	CSlurperApp slurper;
	CSlurpOptions options;

	if (!options.prepareArguments(argc, argv))
		return 0;

	while (!options.commandList.IsEmpty())
	{
		SFString command = nextTokenClear(options.commandList, '\n');
		outErr << "Processing: " << command << "\n";

		options.Init(); // reinitialize
		if (!options.parseArguments(command))
			return FALSE;

		SFString message;

		// Setup the slurper
		if (!slurper.Initialize(options, message))
			return usage(message);

		// Slurp the address...
		if (!slurper.Slurp(options, message))
			return usage(message);

		// Apply the filters if any...
		if (!slurper.Filter(options, message))
			return usage(message);

		// Report on the address...
		if (!slurper.Display(options, message))
			return usage(message);
	}
	return FALSE;
}

//---------------------------------------------------------------------------------------------------
SFBool CSlurperApp::Initialize(CSlurpOptions& options, SFString& message)
{
	// This allows us to spin through these classes' lists of fields without explicit display strings
	CFunction::registerClass();
	CParameter::registerClass();
	CAccount::registerClass();
	CTransaction::registerClass();
	CReceipt::registerClass();
	CTrace::registerClass();
	CLogEntry::registerClass();
	CStructLog::registerClass();
	HIDE_FIELD(CTransaction, "toContract");
	HIDE_FIELD(CTransaction, "receipt");
	HIDE_FIELD(CTransaction, "traces");

	// If this is the first time we've ever run, build the config file
	if (!establishFolders(config, version.toString()))
	{
		message = "Unable to create data folders at " + cachePath();
		return FALSE;
	}

	// Note this may not return if user chooses to exit
	api.checkKey(config);

	// If we are told to get the address from the rerun address, and the
	// user hasn't supplied one, do so...
	SFString addr = options.addr;
	if (addr.IsEmpty() && options.rerun)
		addr = config.GetProfileStringGH("SETTINGS", "rerun", EMPTY);

	// Ethereum addresses are case insensitive. Force all address to lower case
	// to avoid mismatches with Mist browser for example
	addr.MakeLower();

	// We can't run without an address...
	if (addr.IsEmpty())
	{
		message = "You must supply an Ethereum account or contract address. ";
		if (!options.rerun)
			message += "Use -r flag to rerun the most recently slurped address.";
		return FALSE;
	}

	if (options.wantsArchive)
	{
		if (options.archiveFile.IsEmpty() && options.name.IsEmpty())
			return usage("-a and -n may not both be empty. Specify either an archive file or a name. Quitting...");

		SFString fn = (options.name.Contains("/") ? options.name : options.exportFormat + "/" + options.name) + (options.name.Contains(".")?"":"." + options.exportFormat);
		SFFile file(fn);
		if (options.archiveFile.IsEmpty())
			options.archiveFile = file.getFullPath();
		ASSERT(options.output==NULL);
		options.output = fopen((const char*)options.archiveFile, asciiWriteCreate);
		if (!options.output)
			return usage("file '" + options.archiveFile + "' could not be opened. Quitting.");
		outScreen.setOutput(options.output);
	}

	// Save the address and name for later
	config.SetProfileString("SETTINGS", "rerun", addr);
	config.writeFile(version.toString());

	// Load per address configurations if any
	SFString customConfig = configPath("configs/"+addr+".conf");
	if (SFos::fileExists(customConfig) || !options.name.IsEmpty())
	{
		CConfig perAddr(NULL);
		perAddr.setFilename(customConfig);
		if (SFos::fileExists(customConfig))
		{
			perAddr.loadFile(customConfig);
			config.mergeConfig(&perAddr);
		}
		if (!options.name.IsEmpty())
		{
			perAddr.SetProfileString("SETTINGS", "name", options.name);
			perAddr.writeFile(version.toString());
		}
	}

	// Rerun will not reload the cache if it's already in memory
	if (theAccount.addr == addr)
		options.rerun = TRUE;

	// If we're not re-running, we're slurping and we need an empty transaction list
	if (!options.rerun)
	{
		theAccount.transactions.Clear();
		theAccount = CAccount();
extern SFInt32 nAbis;
		nAbis=0;
	}

	// We are ready to slurp
	theAccount.addr = addr;

	outErr << "\t" << "Slurping " << theAccount.addr << "\n";

	// Finished
	return TRUE;
}

//--------------------------------------------------------------------------------
SFBool CSlurperApp::Slurp(CSlurpOptions& options, SFString& message)
{
	double start = vrNow();

	// Do we have the data for this address cached?
	SFString cacheFilename = cachePath(theAccount.addr+".bin");
	SFBool needToRead = SFos::fileExists(cacheFilename);
	if (options.rerun && theAccount.transactions.getCount())
		needToRead=FALSE;
	if (needToRead)
	{
		// Once a transaction is on the blockchain, it will never change
		// therefore, we can store them in a binary cache. Here we read
		// from a previously stored cache.
		SFArchive archive(TRUE, NO_SCHEMA);
		if (!archive.Lock(cacheFilename, binaryReadOnly, LOCK_NOWAIT))
		{
			message = "Could not open file: '" + cacheFilename + "'\n";
			return options.fromFile;
		}
		theAccount.Serialize(archive);
		archive.Close();
	}

	SFTime now = Now();
	SFTime fileTime = SFos::fileLastModifyDate(cacheFilename);

	// If the user tells us he/she wants to update the cache, or the cache
	// hasn't been updated in five minutes, then update it
	SFInt32 nSeconds = MAX(60,config.GetProfileIntGH("SETTINGS", "update_freq", 300));
	if ((now - fileTime) > SFTimeSpan(0,0,0,nSeconds))
	{
		// This is how many records we currently have
		SFInt32 origCount  = theAccount.transactions.getCount();
		SFInt32 nNewBlocks = 0;
		SFInt32 nextRecord = origCount;

		outErr << "\tSlurping new transactions from blockchain...\n";
		SFInt32  nRequests = 0, nRead = 0;

		// We already have 'page' pages, so start there.
		SFInt32  page = MAX(theAccount.lastPage,1);

		// Keep reading until we get less than a full page
		SFString contents;
		SFBool done = FALSE;
//#define NO_INTERNET
#ifdef NO_INTERNET
		done = TRUE;
#endif
		while (!done)
		{
			SFString url = SFString("https://api.etherscan.io/api?module=account&action=txlist&sort=asc") +
						"&address=" + theAccount.addr +
						"&page="    + asString(page) +
						"&offset="  +
						asString(options.pageSize) +
						"&apikey="  + api.getKey();

			// Grab a page of data from the web api
			SFString thisPage = urlToString(url);

			// See if it's good data, if not, bail
			message = nextTokenClear(thisPage, '[');
			if (!message.Contains("{\"status\":\"1\",\"message\":\"OK\""))
			{
				if (message.Contains("{\"status\":\"0\",\"message\":\"No transactions found\",\"result\":"))
					message = "No transactions were found for address '" + theAccount.addr + "'. Is it correct?";
				return options.fromFile;
			}

			contents += thisPage;

			SFInt32 nRecords = countOf('}',thisPage)-1;
			nRead += nRecords;
			outErr << "\tDownloaded " << nRead << " potentially new transactions." << (isTesting?"\n":"\r");

			// If we got a full page, there are more to come
			done = (nRecords < options.pageSize);
			if (!done)
				page++;

			// Etherscan.io doesn't want more than five pages per second, so sleep for a second
			if (++nRequests==4)
			{
				SFos::sleep(1.0);
				nRequests=0;
			}

			// Make sure we don't spin forever
			if (nRead >= options.maxTransactions)
				done=TRUE;
		}

		SFInt32 minBlock=0,maxBlock=0;
		findBlockRange(contents, minBlock, maxBlock);
#ifndef NO_INTERNET
		outErr << "\n\tDownload contains blocks from " << minBlock << " to " << maxBlock << "\n";
#endif

		// Keep track of which last full page we've read
		theAccount.lastPage = page;
		theAccount.pageSize = options.pageSize;

		// pre allocate the array
		theAccount.transactions.Grow(nRead);

		SFInt32 lastBlock=0;
		char *p = cleanUpJson((char *)(const char*)contents);
		while (p && *p)
		{
			CTransaction trans;SFInt32 nFields=0;
			trans.pParent = &theAccount;
			p = trans.parseJson(p,nFields);
			if (nFields)
			{
				SFInt32 transBlock = trans.blockNumber;
				if (transBlock > theAccount.lastBlock) // add the new transaction if it's in a new block
				{
					theAccount.transactions[nextRecord++] = trans;
					lastBlock = transBlock;
					if (!(++nNewBlocks%REP_FREQ))
					{
						outErr << "\tFound new transaction at block " << transBlock << ". Importing..." << (isTesting?"\n":"\r");
						outErr.Flush();
					}
				}
			}
		}
		if (!isTesting && nNewBlocks) { outErr << "\tFound new transaction at block " << lastBlock << ". Importing...\n"; outErr.Flush(); }
		theAccount.lastBlock = MAX(theAccount.lastBlock, lastBlock);
		// Write the data if we got new data
		SFInt32 newRecords = (theAccount.transactions.getCount() - origCount);
		if (newRecords)
		{
			outErr << "\tWriting " << newRecords << " new records to cache\n";
			SFArchive archive(FALSE, NO_SCHEMA);
			if (archive.Lock(cacheFilename, binaryWriteCreate, LOCK_CREATE))
			{
				theAccount.transactions.Sort(sortTransactionsForWrite);
				theAccount.Serialize(archive);
				archive.Close();

			} else
			{
				message = "Could not open file: '" + cacheFilename + "'\n";
				return options.fromFile;
			}
		}
	}

	if (!isTesting)
	{
		double stop = vrNow();
		double timeSpent = stop-start;
		fprintf(stderr, "\tLoaded %ld total records in %f seconds\n", theAccount.transactions.getCount(), timeSpent);
		fflush(stderr);
	}

	return (options.fromFile || theAccount.transactions.getCount()>0);
}

//--------------------------------------------------------------------------------
SFBool CSlurperApp::Filter(CSlurpOptions& options, SFString& message)
{
	double start = vrNow();

	SFInt32 nFuncFilts=0;
	SFString funcFilts[20];
	SFString filtList = options.funcFilter;
	while (!filtList.IsEmpty())
		funcFilts[nFuncFilts++] = nextTokenClear(filtList, ',');

	theAccount.nVisible=0;
	for (int i=0;i<theAccount.transactions.getCount();i++)
	{
		CTransaction *trans = &theAccount.transactions[i];
		trans->pParent = &theAccount;

		// Turn every transaction on and then turning them off if they match the filter.
		trans->setShowing(TRUE);

		// The -blocks and -dates filters are mutually exclusive, -dates predominates.
		if (options.firstDate != earliestDate || options.lastDate != latestDate)
		{
			SFTime date = trans->m_transDate;
			SFBool isVisible = (date >= options.firstDate && date <= options.lastDate);
			trans->setShowing(isVisible);

		} else if (options.firstBlock2Read!=0||options.lastBlock2Read!=LONG_MAX)
		{
			SFInt32 bN = trans->blockNumber;
			SFBool isVisible = (bN >= options.firstBlock2Read && bN <= options.lastBlock2Read);
			trans->setShowing(isVisible);
		}

		// The -incomeOnly and -expensesOnly filters are also mutually exclusive
		ASSERT(!(options.incomeOnly && options.expenseOnly)); // can't be both
		if (options.incomeOnly && trans->to != theAccount.addr)
		{
			if (verbose)
				outErr << trans->Format("\tskipping expenditure [{HASH}]\n");
			trans->setShowing(FALSE);

		} else if (options.expenseOnly && trans->from != theAccount.addr)
		{
			if (verbose)
				outErr << trans->Format("\tskipping inflow [{HASH}]\n");
			trans->setShowing(FALSE);
		}

		if (!options.funcFilter.IsEmpty())
		{
			SFBool show = FALSE;
			for (int i=0;i<nFuncFilts;i++)
				show = (show || trans->isFunction(funcFilts[i]));
			trans->setShowing(show);
		}

		// We only apply this if another filter has not already hidden the transaction
		if (trans->isShowing() && options.errFilt)
		{
			// The filter is either equal to '2' (errOnly) in which case
			// we show only errors. Otherwise, show only non-errors.
			SFBool isError = toLong(trans->Format("[{ISERROR}]"));
			trans->setShowing(options.errFilt==2 ? isError : !isError);
		}

		theAccount.nVisible += trans->isShowing();
		SFInt32 nFiltered = (theAccount.nVisible+1);
		if (!(nFiltered%REP_INFREQ)) { outErr << "\t" << "Filtering..." << nFiltered << " records passed." << (isTesting?"\n":"\r"); outErr.Flush(); }
	}

	if (!isTesting)
	{
		double stop = vrNow();
		double timeSpent = stop-start;
		fprintf(stderr, "\tFilter passed %ld visible records of %ld in %f seconds\n", theAccount.nVisible, theAccount.transactions.getCount(), timeSpent);
		fflush(stderr);
	}

	return TRUE;
}

//---------------------------------------------------------------------------------------------------
SFBool CSlurperApp::Display(CSlurpOptions& options, SFString& message)
{
	double start = vrNow();
	if (options.reverseSort)
		theAccount.transactions.Sort(sortReverseChron);
	theAccount.Format(outScreen, getFormatString(options, "file", FALSE));

	if (!isTesting)
	{
		double stop = vrNow();
		double timeSpent = stop-start;
		fprintf(stderr, "\tExported %ld records in %f seconds             \n\n", theAccount.nVisible, timeSpent);
		fflush(stderr);
	}
	return TRUE;
}

//--------------------------------------------------------------------------------
SFString CSlurperApp::getFormatString(CSlurpOptions& options, const SFString& which, SFBool ignoreBlank)
{
	if (which == "file")
		buildDisplayStrings(options);

	SFString errMsg;

	SFString formatName = "fmt_" + options.exportFormat + "_" + which;
	SFString ret = config.GetProfileStringGH("DISPLAY_STR", formatName, EMPTY);
	if (ret.Contains("file:"))
	{
		SFString file = ret.Substitute("file:",EMPTY);
		if (!SFos::fileExists(file))
			errMsg = SFString("Formatting file '") + file + "' for display string '" + formatName + "' not found. Quiting...\n";
		else
			ret = asciiFileToString(file);

	} else if (ret.Contains("fmt_")) // it's referring to another format string...
	{
		SFString newName = ret;
		ret = config.GetProfileStringGH("DISPLAY_STR", newName, EMPTY);
		formatName += ":"+newName;
	}
	ret = ret.Substitute("\\n","\n").Substitute("\\t","\t");

	// some sanity checks
	if (countOf('{',ret) != countOf('}',ret) || countOf('[',ret) != countOf(']',ret))
	{
		errMsg = SFString("Mismatched brackets in display string '") + formatName + "': '" + ret + "'. Quiting...\n";

	} else if (ret.IsEmpty() && !ignoreBlank)
	{
		errMsg = SFString("Empty display string '") + formatName + "'. Quiting...\n";
	}

	if (!errMsg.IsEmpty())
	{
		outErr << errMsg;
		exit(0);
	}

	return ret;
}

//---------------------------------------------------------------------------------------------------
void CSlurperApp::buildDisplayStrings(CSlurpOptions& options)
{
	// Set the default if it's not set
	if (options.exportFormat.IsEmpty())
		options.exportFormat = "json";

	// This is what we're really after...
	const SFString fmtForRecords = getFormatString(options, "record", FALSE);
	ASSERT(!fmtForRecords.IsEmpty());

	// ...we may need this to build it.
	const SFString fmtForFields  = getFormatString(options, "field", !fmtForRecords.Contains("{FIELDS}"));
	ASSERT(!fmtForFields.IsEmpty());

	SFString defList = config.GetProfileStringGH("DISPLAY_STR", "fmt_fieldList", EMPTY);
	SFString fieldList = config.GetProfileStringGH("DISPLAY_STR", "fmt_"+options.exportFormat+"_fieldList", defList);
	if (fieldList.IsEmpty())
		fieldList = GETRUNTIME_CLASS(CTransaction)->listOfFields();
	SFString origList = fieldList;

	theAccount.displayString = EMPTY;
	theAccount.header        = EMPTY;
	while (!fieldList.IsEmpty())
	{
		SFString fieldName = nextTokenClear(fieldList, '|');
		SFBool force = fieldName.Contains("*");fieldName.Replace("*",EMPTY);

		const CFieldData *field = GETRUNTIME_CLASS(CTransaction)->FindField(fieldName);
		if (!field)
		{
			outErr << "Field '" << fieldName << "' not found in fieldList '" << origList << "'. Quitting...\n";
			exit(0);
		}
		if (field->isHidden() && force) ((CFieldData*)field)->setHidden(FALSE);
		if (!field->isHidden())
		{
			SFString resolved = fieldName;
			if (options.exportFormat!="json")
				resolved = config.GetProfileStringGH("FIELD_STR", fieldName, fieldName);
			theAccount.displayString += fmtForFields.Substitute("{FIELD}", "{" + toUpper(resolved)+"}").Substitute("{p:FIELD}", "{p:"+resolved+"}");
			theAccount.header        += fmtForFields.Substitute("{FIELD}", resolved).Substitute("[",EMPTY).Substitute("]",EMPTY).Substitute("<td ","<th ");
		}
	}
	theAccount.displayString = StripAny(theAccount.displayString, "\t ");
	theAccount.header = StripAny(theAccount.header, "\t ");

	theAccount.displayString = Strip(fmtForRecords.Substitute("[{FIELDS}]", theAccount.displayString), '\t');
	theAccount.displayString.ReplaceAll("[{NAME}]", options.archiveFile);
	if (options.exportFormat=="json")
	{
		// One little hack to make raw json more readable
		theAccount.displayString.ReplaceReverse("}]\",","}]\"\n");
		if (options.prettyPrint)
		{
			theAccount.displayString.ReplaceAll("\"[{p:", "            \"[{p:");
			theAccount.displayString.ReplaceAll("}]\",",  "}]\",\n");
			theAccount.displayString.ReplaceAll("\":\"", "\": \"");
		}
	}
}

//--------------------------------------------------------------------------------
void findBlockRange(const SFString& json, SFInt32& minBlock, SFInt32& maxBlock)
{
	SFString search = "\"blockNumber\":\"";
	SFInt32  len = search.GetLength();

	minBlock = 0;
	SFInt32 first = json.Find(search);
	if (first!=-1)
	{
		SFString str = json.Mid(first+len,100);
		minBlock = toLong(str);
		//		outScreen << first << " : " << str << " : " << minBlock << "\n";
	}

	SFString end = json.Mid(json.ReverseFind('{'),10000);//pull off the last transaction
	SFInt32 last = end.Find(search);
	if (last!=-1)
	{
		SFString str = end.Mid(last+len,100);
		maxBlock = toLong(str);
		//		outScreen << last << " : " << str << " : " << maxBlock << "\n";
	}
}

//--------------------------------------------------------------------------------
// Make sure our data folder exist, if not establish it
SFBool establishFolders(CConfig& config, const SFString& vers)
{
	// Just double check we're in the right bracnh
	if (getCWD().Contains("/src.GitHub.1"))
	{
		outErr << "You're in src.GitHub.1 folder. Comment this code if you mean to be.\n";
		exit(0);
	}

	SFString configFilename = configPath("ethslurp.conf");
	config.setFilename(configFilename);
	if (SFos::folderExists(cachePath()) && SFos::fileExists(configFilename))
	{
		config.loadFile(configFilename);
		return TRUE;
	}

	// create the main folder
	SFos::mkdir(configPath());
	if (!SFos::folderExists(configPath()))
		return FALSE;

	// create the folder for the slurps
	SFos::mkdir(cachePath());
	if (!SFos::folderExists(cachePath()))
		return FALSE;

	config.SetProfileString("SETTINGS",     "cachePath",         cachePath());
	config.SetProfileString("SETTINGS",     "api_key",           EMPTY);

	config.SetProfileString("DISPLAY_STR",  "fmt_fieldList",     EMPTY);

	config.SetProfileString("DISPLAY_STR",  "fmt_txt_file",      "[{HEADER}]\\n[{RECORDS}]");
	config.SetProfileString("DISPLAY_STR",  "fmt_txt_record",    "[{FIELDS}]\\n");
	config.SetProfileString("DISPLAY_STR",  "fmt_txt_field",     "\\t[{FIELD}]");

	config.SetProfileString("DISPLAY_STR",  "fmt_csv_file",      "[{HEADER}]\\n[{RECORDS}]");
	config.SetProfileString("DISPLAY_STR",  "fmt_csv_record",    "[{FIELDS}]\\n");
	config.SetProfileString("DISPLAY_STR",  "fmt_csv_field",     "[\"{FIELD}\"],");

	config.SetProfileString("DISPLAY_STR",  "fmt_html_file",     "<table>\\n[{HEADER}]\\n[{RECORDS}]</table>\\n");
	config.SetProfileString("DISPLAY_STR",  "fmt_html_record",   "\\t<tr>\\n[{FIELDS}]</tr>\\n");
	config.SetProfileString("DISPLAY_STR",  "fmt_html_field",    "\\t\\t<td>[{FIELD}]</td>\\n");

	config.SetProfileString("DISPLAY_STR",  "fmt_json_file",     "[{RECORDS}]\\n");
	config.SetProfileString("DISPLAY_STR",  "fmt_json_record",   "\\n        {\\n[{FIELDS}]        },");
	config.SetProfileString("DISPLAY_STR",  "fmt_json_field",    "\"[{p:FIELD}]\":\"[{FIELD}]\",");

	config.SetProfileString("DISPLAY_STR",  "fmt_custom_file",	 "file:custom_format_file.html");
	config.SetProfileString("DISPLAY_STR",  "fmt_custom_record", "fmt_html_record");
	config.SetProfileString("DISPLAY_STR",  "fmt_custom_field",	 "fmt_html_field");

	config.writeFile(vers);
	return SFos::fileExists(config.getFilename());
}

//----------------------------------------------------------------------------------------
int sortReverseChron(const void *rr1, const void *rr2)
{
        CTransaction *tr1 = (CTransaction*)rr1;
        CTransaction *tr2 = (CTransaction*)rr2;

        SFInt32 ret;
        ret = tr2->timeStamp - tr1->timeStamp;         if (ret!=0) return (int)ret;
        return sortTransactionsForWrite(rr1,rr2);
}
