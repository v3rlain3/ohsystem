<html>
<head>
<title>Bot Status</title>
<style>
<style>
body {
    margin-top: 20px;
    margin-left: 40px;
    font-family: 'trebuchet MS', 'Lucida sans', Arial;
    font-size: 14px;
    color: #444;
}

.bot { display: inline-block; float: left; width: 10%; min-width: 250px; height: 215px; color: #fff; border: 1px solid #222; border-bottom: 1px solid #aaa; background-color: #088A08; margin: 10px; padding: 5px; font-size: 90%;}
.bot.warning { background-color: #D7DF01; color: #000;}
.bot.danger { background-color: #DF7401}
.bot.error { background-color: #B40404}

</style>
</style>
</head>
<body>
<h1>Bot Status</h1>

<?php
mysql_connect("localhost", "root", "YOUR_PASSWORD");
mysql_select_db("ohsystem");

$result = mysql_query("SELECT * FROM oh_bot_status WHERE last_update >= addtime(now(), '-00:01:00')");

while($row = mysql_fetch_array($result)) {
 $count = 0;
 if($row['Europe']!=1)
  $count++;
 if($row['USEast']!=1)
  $count++;
 if($row['USWest']!=1)
  $count++;
 if($row['Asia']!=1)
  $count++;
 if($row['XAMP']!=1)
  $count++;
 if($row['Garena']!=1)
  $count++;
 if($row['WC3Connect']!=1)
  $count++;
?>
 <div class="bot <?php echo ( $count != 0 ? ($count < 2 ? 'warning' : ($count < 3 ? 'danger' : 'error' ) ) : '' ) ?>"><b>Bot ID:</b> #<?=$row['botid']?><br><b>Username: </b><?=$row['name']?><br><b>GameName: </b><?=$row['gamename']?><br>
  <b>Hosted IP:Port: </b><?=$row['ip']?>:<?=$row['hostport']?><br><br>

  <b>Europe: </b><?=$row['Europe']?><br><b>USEast: </b><?=$row['USEast']?><br><b>USWest: </b><?=$row['USWest']?><br><b>Asia: </b><?=$row['Asia']?><br><b>XAMP: </b><?=$row['XAMP']?><br><b>Garena: </b><?=$row['Garena']?><br>
  <b>WC3Connect: </b><?=$row['WC3Connect']?>
 </div>
<? } ?>
<div style="clear: both"></div>
<br>

Bot Status:<br>
0 - Not initalized<br>
1 - Connected<br>
2 - IP banned<br>
3 - disconnected (timed out)<br>
4 - wrong ROC key or in use<br>
5 - wrong TFT key or in use<br>
6 - invalid username or password<br>
7 - invalid game version<br>
8 - cd keys not accepted
</body>
</html>

