<?php
$lasttime = 0;
while(1)
{
	if(time() - $lasttime < 2)
		sleep(2);
	$lasttime = time();

	echo "Getting files...\n";
	$files = `ls data`;
	if($files == "\n")
		continue;
	//echo $files;
	$file_array = explode("\n", $files);
	//print_r($file_array);
	$file_no = array();
	foreach($file_array as $row) {
		if(!strstr($row, ".txt"))
			continue;

		$t = explode(".", $row);
		$c = $t[0];
		$file_no[] = $c;
	}
	if(!count($file_no))
		continue;

	echo "Moving files...\n";
	// move to scan dir
	foreach($file_no as $row) {
		echo $row." ";
		rename("data/".$row, "scan/".$row);
		rename("data/".$row.".txt", "scan/".$row.".txt");
	}
	echo "\n";
	$count = count($file_no);
	echo "Scanning {$count} files...\n";
	// scan it
	$result = `clamdscan scan/ | grep FOUND`;
	if($result == "")
		continue;
	$av_array = explode("\n", $result);
	//print_r($av_array);
	foreach($av_array as $row) {
		if(!strstr($row, "FOUND"))
			continue;

		$t = explode(" ", $row);
		$t2 = explode("/", substr($t[0], 0, strlen($t[0])-1));
		$file = $t2[count($t2)-1];
		$conf = file_get_contents("scan/".$file.".txt");
		$s = explode(",", $conf);
		$virus = $t[1];
		$string = "Src {$s[0]}:{$s[1]} Dst {$s[2]}:{$s[3]} Virus {$virus} URL {$s[4]}\n";
		echo $string;
		`echo "$string" | nc 127.0.0.1 3838`;
	}
	
	`rm -rf scan/*`;
}

