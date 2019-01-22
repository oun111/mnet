
/**
 * 
 */

/**
 * @author user1
 *
 */

class Request extends pktBase {
  byte shortVer ;
  
  int version ;
  
  int len ;
  
  byte[] msg ;
  
  byte magic ;
  
  long time ;
  
  static final int maxMsgLength = 64 ;

  
  Request() { 
	  msgLength = maxMsgLength ;
  }
  
  Request(byte data[]) throws Exception {
	  msgLength = maxMsgLength ;

	  toObject(data);
  }
  
  
/**
 * @return the version
 */
public final int getVersion() {
	return version;
}

/**
 * @param version the version to set
 */
public final void setVersion(int version) {
	this.version = version;
}

/**
 * @return the shortVer
 */
public final byte getShortVer() {
	return shortVer;
}

/**
 * @param shortVer the shortVer to set
 */
public final void setShortVer(byte shortVer) {
	this.shortVer = shortVer;
}

/**
 * @return the len
 */
public final int getLen() {
	return len;
}

/**
 * @param len the len to set
 */
public final void setLen(int len) {
	this.len = len;
}


/**
 * @return the msg
 */
public final byte[] getMsg() {
	return msg;
}

/**
 * @param msg the msg to set
 */
public final void setMsg(byte[] msg) {
	this.msg = msg;
}

/**
 * @return the magic
 */
public final byte getMagic() {
	return magic;
}

/**
 * @param magic the magic to set
 */
public final void setMagic(byte magic) {
	this.magic = magic;
}

/**
 * @return the time
 */
public final long getTime() {
	return time;
}

/**
 * @param time the time to set
 */
public final void setTime(long time) {
	this.time = time;
}

  
} 
