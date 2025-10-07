(
// see https://github.com/testmoapp/junitxml

~parseTestSuite = {|testSuiteName, results|
	var string;
	var numTests = results.size;
	// each test run stores its assertion results in an array
	var numAssertions = results.sum({|r| (r[\results] ? []).size});
	var numFailures = results.sum({|r|
		// we therefore try to access its first and provide
		// an empty dict in case there are no runs
		(r[\results] ? []).count({|assertion| assertion[\pass].asBoolean.not});
	});
	var numSkipped = results.count({|r| r[\skip] == true});
	var time = results.sum({|r| r[\duration] ? 0.0});

	var resultStrings = results.collect({|result|
		var failures = (result[\results] ? []).select({|assertion|
			assertion[\pass].asBoolean.not;
		});
		var testString = "<testcase name=\"%\" classname=\"%\" assertions=\"%\" time=\"%\">%%</testcase>".format(
			result[\test],
			testSuiteName,
			(result[\results] ? []).size,
			result[\duration] ? 0.0,
			// failure string
			failures.collect({|failure|
				"<failure message=\"%\"></failure>".format(failure[\test]);
			}).join(""),
			// skip string
			if(result[\skipReason].notNil, {
				"<skipped message=\"%\" />".format(result[\skipReason]);
			},{
				""
			});
		);
		testString;
	});

	var testString = "<testsuite name=\"%\" tests=\"%\" failures=\"%\" skipped=\"%\" assertions=\"%\" time=\"%\">\n%\n</testsuite>".format(
		testSuiteName,
		numTests,
		numFailures,
		numSkipped,
		numAssertions,
		time,
		resultStrings.join("\n"),
	);

	(
		\numTests: numTests,
		\numAssertions: numAssertions,
		\numFailures: numFailures,
		\numSkipped: numSkipped,
		\time: time,
		\xmlString: testString,
	);
};

// groups flat list of test results by test suite
~regroupResults = {|results|
	var groupedResults = ();

	results.do({|result|
		var suite = result[\suite].asSymbol;
		if(groupedResults.keys.includes(suite), {
			groupedResults[suite].add(result);
		}, {
			groupedResults[suite] = [result];
		});
	});

	groupedResults;
};

~createJunitXML = {|path|
	var results = Object.readArchive(path);
	var groupedResults = ~regroupResults.value(results);

	var numTests = 0;
	var numFailures = 0;
	var numSkipped = 0;
	var aggregatedTime = 0.0;

	var testSuites = [];
	var xmlString;

	groupedResults.pairsDo({|name, results|
		testSuites = testSuites.add(~parseTestSuite.(name, results));
	});

	xmlString = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>
<testsuites name=\"ClassLibrary\" tests=\"%\" failures=\"%\" skipped=\"%\" time=\"%\">%\n</testsuites>".format(
		testSuites.sum({|t| t[\numTests] }),
		testSuites.sum({|t| t[\numFailures] }),
		testSuites.sum({|t| t[\numSkipped] }),
		testSuites.sum({|t| t[\time] }),
		testSuites.collect({|t| t[\xmlString] }).join("\n"),
	);

	xmlString;
};

// if we are running this script outside of the IDE we want to
// quit the interpreter
~isRunningInIDE = \Document.asClass.isNil;

~archivePath = thisProcess.argv[0] ? "ARCHIVE_PATH".getenv;
~xmlPath = thisProcess.argv[1] ? "XML_PATH".getenv;

"Writing JUnit XML of '%'' to '%'".format(~archivePath, ~xmlPath).postln;

~xmlContent = ~createJunitXML.(~archivePath);

File.use(~xmlPath.absolutePath, "w", {|f| f.write(~xmlContent)});

"Finished writing JUnit XML".postln;

if(~isRunningInIDE.not, {0.exit});
)
