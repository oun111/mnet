/**
 * 
 */

/**
 * @author user1
 *
 */
public class Response extends pktBase {
	
	static final int maxMsgLength = 256 ;
	
	int version ;
	
	byte[] rspMsg; // maxMsgLength bytes
	
	

	Response() { 
		msgLength = maxMsgLength ;
	}
	
	Response(byte data[]) throws Exception {
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
	 * @return the rspMsg
	 */
	public final byte[] getRspMsg() {
		return rspMsg;
	}

	/**
	 * @param rspMsg the rspMsg to set
	 */
	public final void setRspMsg(byte[] rspMsg) {
		this.rspMsg = rspMsg;
	}
	

}
