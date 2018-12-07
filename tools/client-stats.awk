# combines results from a dashclient log and the proxy log
#
# usage awk -f client-stats,awk dashclient-<n>.log proxy.log

 BEGIN {
	 SUBSEP = "|"
	 next_segment = 1
	 start = 0
}

/starting/ {
	# matches dashclient-*.log
	video = $5	# save current video name
	start = 0	# reset elapsed time
}

/DAG/ {
	# matches dashclient-*.log

	# I'm sure there is a better way to do this, but brute force works
	data = $5
	gsub("\\?bandwidth=", " ", data)
	gsub("http://DAG.0.-.", "", data)
	gsub("\&client=", " ", data)
	gsub("\\$", ":", data)

	split(data, parts)
	cid = parts[1]


	split($3, t, ":")
	s = t[2] * 60 + t[3]
	if (start == 0) {
		start = s
	}

	# array of cids in download order
	segments[next_segment] = cid
	next_segment++

	# mapping from cid to the video it belongs to
	chunks[cid] = video

	# stats per chunk
	stats[cid, "elapsed"] = s - start
	stats[cid, "bitrate"] = parts[2]
	stats[cid, "client"] = parts[3]
	stats[cid, "bytes"] = ""
	stats[cid, "duration"] = ""
	stats[cid, "cached"] = 0
}

$5 ~ /bytes/ {
	# matches dashclient-*.log

	# these are on a different line in the client log, but will always go with
	# the cid that preceeds it in the log
	stats[cid, "bytes"] = $4
	stats[cid, "duration"] = $6
	stats[cid, "speed"] = $8
}

/cached/ {
	# this comes from proxy.log

	# unfortunately this log is a bit spare, so we don't know which client was pulling the chunk
	# assume that the cached status is the same for all clients until the log has more info
	gsub(":$", "", $1)
	stats[$1, "cached"] = $3
}


END {
	print "time,video,cid,status,bitrate,client,cached,bytes,duration,speed"

	# make output suitable for a .csv file
	for (segment = 1; segment < next_segment; segment++) {
		cid = segments[segment]
		status = stats[cid, "bytes"] == "" ? "0" : "1"
		print stats[cid, "elapsed"] "," chunks[cid] "," cid "," status "," stats[cid, "bitrate"] "," stats[cid, "client"] "," stats[cid, "cached"] "," stats[cid, "bytes"] "," stats[cid, "duration"] "," stats[cid, "speed"]
	}
}
