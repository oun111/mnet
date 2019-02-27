

create table MERCHANT_CONFIGS
(
  ID int not null auto_increment,
  NAME char(48) not null,
  SIGN_TYPE char(16) not null default 'MD5',
  PUBKEY varchar(4096),
  PRIVKEY varchar(4096),
  PARAM_TYPE char(16) not null default 'html',
  primary key(ID)
);


create table CHANNEL_CONFIGS
(
  ID int not null auto_increment,
  NAME char(48) not null,
  REQ_URL varchar(512) not null,
  PARAM_TYPE char(16) not null default 'html',
  PUBLIC_KEY_PATH varchar(512) not null,
  PRIVATE_KEY_PATH varchar(512) not null,
  PRODUCT_CODE char(64) not null,
  TIMEOUT_EXPRESS char(16) not null,
  APP_ID varchar(64) not null,
  METHOD varchar(64) not null,
  FORMAT varchar(32) not null,
  CHARSET varchar(16) not null,
  SIGN_TYPE varchar(16) not null,
  VERSION varchar(8) not null,
  NOTIFY_URL varchar(512) not null,
  RETURN_URL varchar(512) not null,
  primary key(ID)
);

