#!/bin/bash

# written by Chris.McDonald@uwa.edu.au
# thanks to:  https://en.clipdealer.com/vector/media/A:112583666

HOST="127.0.0.1"	# assuming all stations on the same host
LEAVE=`date '+%H:%M'`	# or set to a fixed value

# ---------------------------------------

usage() {
    echo "Usage: $0 startscript output.html"
    exit 1
}

if [ $# != "2" ]; then
    usage
fi
if [ ! -f $1 ]; then
    echo cannot read $1
    usage
fi

# ---------------------------------------

TMP="/tmp/mf-$$"
trap "rm -f $TMP ; exit 1" INT TERM

header() {
cat << THE_END
<html>
<body style="background-image: url('https://images.assetsdelivery.com/compings_v2/zoaarts/zoaarts1810/zoaarts181000012.jpg'); background-repeat: repeat;">

<style>
div.box {
  margin:		1em;
  width:		30em;
  border-radius:	6px;
  border:		1px solid gray;
  background-color:	honeydew;
  padding:		0.4em;
}
</style>

<div class="box">
  <h3>&nbsp;Leaving after $LEAVE</h3>
</div>
THE_END
}

footer() {
cat << THE_END

</body>
</html>
THE_END
}

each_station() {
    while read from tcpport ; do
    echo ; echo '<div class="box">'

cat << THE_END
<form action="http://$HOST:$tcpport/">
  <table>
  <tr>
    <td style="text-align: right;">Leaving from:</td>
    <td style="text-align: left;">&nbsp;<b>$from</b></td>
    <td style="padding-left: 3em;"><i>http://$HOST:$tcpport/</i></td>
  </tr>
  <tr>
    <td style="text-align: right;"><label>Destination:</label></td>
    <td><select name="to">
THE_END
    while read dest _ ; do
	if [ "$dest" != "$from" ]; then
	    echo "      <option value='$dest'>$dest</option>"
	fi
    done < $TMP
cat << THE_END
    </select></td>
    <td style="padding-left: 3em;"><input type="submit" value=" Let's go! "></td>
  </tr>
  </table>
<!--
  <input type="hidden" name="leave" value="$LEAVE">
-->
</form>
THE_END
    echo "</div>"

    done < $TMP
}

# ---------------------------------------

echo "reading file $1"
echo "creating file $2"
expand < $1 | sed 's/^ //g' | \
	tr -s ' ' | grep '[a-z]' | cut '-d ' -f2,3 | sort > $TMP
( header ; each_station ; footer ) > $2

rm -f $TMP
