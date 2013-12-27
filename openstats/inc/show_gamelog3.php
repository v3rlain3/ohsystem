<?php
if (!isset($website) ) { header('HTTP/1.1 404 Not Found'); die; }

?>
<a name="gamelog"></a>
<div id="LobbyGameLog">

<?php if (!empty($GameLogData[0]["lobbylog"]) ) { ?>
  <div class="LobbyGameLogWrapper">
    <h2><?=$lang["game_log_lobby"]?></h2>
	<div class="LobbyGameLogInner">
	  <?=$GameLogData[0]["lobbylog"]?></div>
  </div>
<?php } ?>

<?php if (!empty($GameLogData[0]["gamelog"]) ) { ?>
  <div class="LobbyGameLogWrapper">
    <h2><?=$lang["game_log_game"]?></h2>
	<div class="LobbyGameLogInner">
	  <?=$GameLogData[0]["gamelog"]?></div>
  </div>
<?php } ?>

</div>