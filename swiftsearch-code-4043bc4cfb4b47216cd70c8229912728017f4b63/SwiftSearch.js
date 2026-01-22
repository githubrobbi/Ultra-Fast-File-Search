String.prototype.endsWith = function (str) {
	return str.length <= this.length && this.slice(this.length - str.length) === str;
}
var WshShell = new ActiveXObject("WScript.Shell");
var WshProcEnv = WshShell.Environment("Process");
var FileSystemObject = new ActiveXObject("Scripting.FileSystemObject");
var ForReading = 1, ForWriting = 2, TristateTrue = -1, TristateFalse = 0;
var
	SCRIPT_PATH      = WScript.ScriptFullName,
	SCRIPT_DIRECTORY = FileSystemObject.GetParentFolderName(SCRIPT_PATH),
	SCRIPT_NAME      = WScript.ScriptName;
	SCRIPT_BASE_NAME = FileSystemObject.GetBaseName(SCRIPT_NAME);
function generate_mui_file(target_path, langid, langname) {
	var target_dir = FileSystemObject.GetParentFolderName(target_path);
	var target_name = FileSystemObject.GetFileName(target_path);
	var target_base_name = FileSystemObject.GetBaseName(target_path);
	var target_ext = FileSystemObject.GetExtensionName(target_path);
	var target_ln_path = FileSystemObject.BuildPath(target_dir, target_base_name + ".ln" + "." + target_ext);
	var target_mui_dir = FileSystemObject.BuildPath(target_dir, langname);
	try {
		FileSystemObject.CreateFolder(target_mui_dir);
	} catch (ex) {
		if (ex.number !== 0x800A003A - Math.pow(2, 32)) {
			throw ex;
		}
	}
	var target_mui_path = FileSystemObject.BuildPath(target_mui_dir, target_name + ".mui");
	var result = system(CreateCommandLine(["MUIRCT", "-q", "MUIConfig.xml", "-g", langid, "-x", langid, target_path, target_ln_path, target_mui_path]));
	if (result === 0) {
		FileSystemObject.DeleteFile(target_ln_path);
	}
	return result;
}
var CALLBACKS = {
	'prelink': function prelink(argv) {
		// Merge STRINGTABLE objects
		var  input_file_name = FileSystemObject.BuildPath(SCRIPT_DIRECTORY, SCRIPT_BASE_NAME) + ".rc";
		var output_file_name = input_file_name;
		var unicode = false;
		var data = null;
		{
			if (input_file_name) {
				var testinfile = FileSystemObject.OpenTextFile(input_file_name, ForReading, false, unicode ? TristateTrue : TristateFalse);
				var data = testinfile.ReadAll();
				if (data.indexOf('\0') >= 0) {
					unicode = true;
					data = null;
				}
				testinfile.Close();
			}
			if (data === null) {
				var infile = input_file_name ? FileSystemObject.OpenTextFile(input_file_name, ForReading, false, unicode ? TristateTrue : TristateFalse) : FileSystemObject.GetStandardStream(0, unicode);
				data = infile.ReadAll();
				infile.Close();
			}
		}
		var changes = 0;
		for (; ; )
		{
			var old = data;
			data = old.replace(/(STRINGTABLE\r\nBEGIN\r\n(?:\s+\w+\s+\"(?:[^\\\"]|\"\"|\\.)*\"\s*\r\n)*)END(?:\r\n)+STRINGTABLE\r\nBEGIN\r\n/g, "$1");
			if (old === data) {
				break;
			}
			++changes;
		}
		if (changes)
		{
			WScript.StdErr.WriteLine("Merging STRINGTABLEs: \"" + input_file_name + "\" -> \"" + output_file_name + "\"");
			{
				var outfile = output_file_name ? FileSystemObject.OpenTextFile(output_file_name, ForWriting, true, unicode ? TristateTrue : TristateFalse) : FileSystemObject.GetStandardStream(1, unicode);
				if (unicode && !output_file_name) {
					outfile.Write("\uFEFF");
				}
				outfile.Write(data);
				outfile.Close();
			}
		}
	},
	'postbuild': function postbuild(argv) {
		var result;
		var iarg = 0;
		var target_path = argv[iarg++], configuration = argv[iarg++], platform = argv[iarg++], langid = argv[iarg++], langname = argv[iarg++];
		var target_dir = FileSystemObject.GetParentFolderName(target_path);
		var target_name = FileSystemObject.GetFileName(target_path);
		WScript.StdErr.WriteLine("Generating MUI file...");
		result = generate_mui_file(target_path, langid, langname);
		if (result === 0 && target_dir.endsWith(FileSystemObject.BuildPath(platform, configuration)) /* path is in expected format */ && (platform === "Win32" || platform === "x86")) {
			WScript.StdErr.WriteLine("Embedding 64-bit executable...");
			FileSystemObject.CopyFile(target_path, target_path.replace(/(\.[^\.:]+)/, "32" + "$1"));
			result = system(CreateCommandLine(["ResHacker", "-addoverwrite", target_path, ",", target_path, ",", FileSystemObject.BuildPath(FileSystemObject.BuildPath(FileSystemObject.BuildPath(FileSystemObject.GetParentFolderName(FileSystemObject.GetParentFolderName(target_dir)), "x64"), configuration), target_name), ",", "BINARY", ",", "AMD64", ",", langid]));
		}
		return result;
	}
};
function CreateCommandLine(argv) {
	var result = [];
	for (var i = 0; i < argv.length; ++i) {
		var s = argv[i];
		if (s) {
			s = s.replace(/(\")/g, "^$1");
			if (s != s.replace(/[\"\^\(\)%!\t ]/g, "\"\1\"")) {
				s = "\"" + s + "\"";
			}
		} else if (s === "") {
			s = "\"\"";
		}
		if (typeof s === 'string') {
			result.push(s);
		}
	}
	return result.join(" ");
}
function system(cmdline) {
	var result;
	var old_path = WshProcEnv("PATH");
	WshProcEnv("PATH") = [
		old_path,
		WshShell.ExpandEnvironmentStrings("%ProgramFiles%\\Resource Hacker"),
		WshShell.ExpandEnvironmentStrings("%ProgramFiles(x86)%\\Resource Hacker"),
		WshShell.ExpandEnvironmentStrings("%ProgramFiles(x86)%\\Microsoft SDKs\\Windows\\v7.1A\\bin\\x64"),
		WshShell.ExpandEnvironmentStrings("%ProgramFiles(x86)%\\Microsoft SDKs\\Windows\\v7.1A\\bin"),
		WshShell.ExpandEnvironmentStrings("%ProgramFiles%\\Microsoft SDKs\Windows\v6.0A\Bin\\x64"),
		WshShell.ExpandEnvironmentStrings("%ProgramFiles%\\Microsoft SDKs\Windows\v6.0A\Bin")
	].join(";")
	try {
		var cmd = "\"" + WshShell.ExpandEnvironmentStrings("%ComSpec%") + "\"" + " /Q /D /S /C \"" + cmdline + "\"";
		result = WshShell.Run(cmd, 0, true);
	} finally {
		WshProcEnv("PATH") = old_path;
	}
	return result;
}
function exec(cmdline) {
	var result = null;
	var process = WshShell.Exec("\"" + WshShell.ExpandEnvironmentStrings("%ComSpec%") + "\"" + " /Q /D /S /C \"" + cmdline + " 2>&1\"");
	if (process !== null) {
		try {
			process.StdIn.Close();
			WScript.StdOut.Write(process.StdOut.ReadAll());
			WScript.StdErr.Write(process.StdErr.ReadAll());
		} finally {
			process.Terminate();
		}
		result = process.ExitCode;
	} else {
		result = -1;
	}
	return result;
}

function main(argv) {
	var result = 0;
	if (argv.length > 0) {
		var key = argv[0].toLowerCase();
		if (key in CALLBACKS) {
			result = CALLBACKS[key](argv.slice(1));
		}
	}
	return result;
}
(function () {
	var argv = [];
	for (var i = 0; i < WScript.Arguments.length; i++) { argv.push(WScript.Arguments(i)); }
	WScript.Quit(main(argv));
})();
